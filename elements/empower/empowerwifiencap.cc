/*
 * empowerwifiencap.{cc,hh} -- encapsultates 802.11 packets (EmPOWER Access Point)
 * John Bicket, Roberto Riggio
 *
 * Copyright (c) 2013 CREATE-NET
 * Copyright (c) 2004 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include "empowerwifiencap.hh"
#include <click/etheraddress.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include <click/packet_anno.hh>
#include <clicknet/llc.h>
#include <click/packet_anno.hh>
#include <elements/wifi/wirelessinfo.hh>
#include "empowerlvapmanager.hh"
CLICK_DECLS

EmpowerWifiEncap::EmpowerWifiEncap() :
		_el(0), _debug(false), _no_stats(false) {
}

EmpowerWifiEncap::~EmpowerWifiEncap() {
}

int EmpowerWifiEncap::configure(Vector<String> &conf,
		ErrorHandler *errh) {

	return Args(conf, this, errh)
			.read_m("EL", ElementCastArg("EmpowerLVAPManager"), _el)
			.read("NO_STATS", _no_stats)
			.read("DEBUG", _debug)
			.complete();

}

void
EmpowerWifiEncap::push(int, Packet *p) {

	if (p->length() < sizeof(struct click_ether)) {
		click_chatter("%{element} :: %s :: packet too small: %d vs %d",
				      this,
				      __func__,
				      p->length(),
				      sizeof(struct click_ether));
		p->kill();
		return;
	}

	click_ether *eh = (click_ether *) p->data();

	EtherAddress src = EtherAddress(eh->ether_shost);
	EtherAddress dst = EtherAddress(eh->ether_dhost);

	// unicast traffic
	if (!dst.is_broadcast() && !dst.is_group()) {
        EmpowerStationState *ess = _el->lvaps()->get_pointer(dst);
        if (!ess) {
			p->kill();
			return;
		}
        if (!ess->_set_mask) {
			p->kill();
			return;
		}
        if (!ess->_authentication_status) {
			click_chatter("%{element} :: %s :: station %s not authenticated",
						  this,
						  __func__,
						  dst.unparse().c_str());
			p->kill();
			return;
		}
        if (!ess->_association_status) {
			click_chatter("%{element} :: %s :: station %s not associated",
						  this,
						  __func__,
						  dst.unparse().c_str());
			p->kill();
			return;
		}

    	if (!_no_stats) {
    		ess->update_tx(p->length());
    	}

        Packet * p_out = wifi_encap(p, dst, src, ess->_lvap_bssid);
		SET_PAINT_ANNO(p_out, ess->_iface_id);
		output(0).push(p_out);
		return;
	}

	// broadcast and multicast traffic
	// note, we need to transmit one frame for each unique bssid. this is due
	// to the fact that we can have the same bssid for multiple LVAPs
	for (LVAPIter it = _el->lvaps()->begin(); it.live(); it++) {
    	if (!it.value()._set_mask) {
    		continue;
    	}
		if (Packet *q = p->clone()) {
	    	if (!_no_stats) {
	    		it.value().update_tx(q->length());
	    	}
			Packet * p_out = wifi_encap(q, dst, src, it.value()._lvap_bssid);
			SET_PAINT_ANNO(p_out, it.value()._iface_id);
			output(0).push(p_out);
		}
	}

	p->kill();

}

Packet *
EmpowerWifiEncap::wifi_encap(Packet *q, EtherAddress dst, EtherAddress src, EtherAddress bssid) {

    WritablePacket *p_out = q->uniqueify();

	if (!p_out) {
		p_out->kill();
		return 0;
	}

	uint8_t mode = WIFI_FC1_DIR_FROMDS;
	uint16_t ethtype;

    memcpy(&ethtype, p_out->data() + 12, 2);

	p_out->pull(sizeof(struct click_ether));
	p_out = p_out->push(sizeof(struct click_llc));

	if (!p_out) {
		p_out->kill();
		return 0;
	}

	memcpy(p_out->data(), WIFI_LLC_HEADER, WIFI_LLC_HEADER_LEN);
	memcpy(p_out->data() + 6, &ethtype, 2);

	if (!(p_out = p_out->push(sizeof(struct click_wifi)))) {
		p_out->kill();
		return 0;
	}

	struct click_wifi *w = (struct click_wifi *) p_out->data();

	memset(p_out->data(), 0, sizeof(click_wifi));
	w->i_fc[0] = (uint8_t) (WIFI_FC0_VERSION_0 | WIFI_FC0_TYPE_DATA);
	w->i_fc[1] = 0;
	w->i_fc[1] |= (uint8_t) (WIFI_FC1_DIR_MASK & mode);

	switch (mode) {
	case WIFI_FC1_DIR_NODS:
		memcpy(w->i_addr1, dst.data(), 6);
		memcpy(w->i_addr2, src.data(), 6);
		memcpy(w->i_addr3, bssid.data(), 6);
		break;
	case WIFI_FC1_DIR_TODS:
		memcpy(w->i_addr1, bssid.data(), 6);
		memcpy(w->i_addr2, src.data(), 6);
		memcpy(w->i_addr3, dst.data(), 6);
		break;
	case WIFI_FC1_DIR_FROMDS:
		memcpy(w->i_addr1, dst.data(), 6);
		memcpy(w->i_addr2, bssid.data(), 6);
		memcpy(w->i_addr3, src.data(), 6);
		break;
	default:
		click_chatter("%{element} :: %s :: invalid mode %d",
				      this,
				      __func__,
				      mode);
		p_out->kill();
		return 0;
	}

	return p_out;

}

enum {
	H_DEBUG
};

String EmpowerWifiEncap::read_handler(Element *e, void *thunk) {
	EmpowerWifiEncap *td = (EmpowerWifiEncap *) e;
	switch ((uintptr_t) thunk) {
	case H_DEBUG:
		return String(td->_debug) + "\n";
	default:
		return String();
	}
}

int EmpowerWifiEncap::write_handler(const String &in_s, Element *e,
		void *vparam, ErrorHandler *errh) {

	EmpowerWifiEncap *f = (EmpowerWifiEncap *) e;
	String s = cp_uncomment(in_s);

	switch ((intptr_t) vparam) {
	case H_DEBUG: {    //debug
		bool debug;
		if (!BoolArg().parse(s, debug))
			return errh->error("debug parameter must be boolean");
		f->_debug = debug;
		break;
	}
	}
	return 0;
}

void EmpowerWifiEncap::add_handlers() {
	add_read_handler("debug", read_handler, (void *) H_DEBUG);
	add_write_handler("debug", write_handler, (void *) H_DEBUG);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(EmpowerWifiEncap)
ELEMENT_REQUIRES(userlevel)
