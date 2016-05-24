#ifndef CLICK_EMPOWERLVAPMANAGER_HH
#define CLICK_EMPOWERLVAPMANAGER_HH
#include <click/config.h>
#include <click/element.hh>
#include <click/timer.hh>
#include <click/etheraddress.hh>
#include <click/hashtable.hh>
#include <clicknet/wifi.h>
#include "empowerrxstats.hh"
#include "empowerpacket.hh"
CLICK_DECLS

/*
=c

EmpowerLVAPManager(HWADDR, EBS, DEBUGFS[, I<KEYWORDS>])

=s EmPOWER

Handles LVAPs and communication with the Access Controller.

=d

Keyword arguments are:

=over 8

=item HWADDR
The raw wireless interface hardware address

=item EBS
An EmpowerBeaconSource element

=item EAUTHR
An EmpowerOpenAuthResponder element

=item EASSOR
An EmpowerAssociationResponder element

=item DEBUGFS
The path to the bssid_extra file

=item EPSB
An EmpowerPowerSaveBuffer element

=item PERIOD
Interval between hello messages to the Access Controller (in msec), default is 5000

=item EDEAUTHR
An EmpowerDeAuthResponder element

=item EDISASSOR
An EmpowerDisassocResponder element

=item DEBUG
Turn debug on/off

=back 8

=a EmpowerLVAPManager
*/

typedef HashTable<uint16_t, uint32_t> CBytes;
typedef CBytes::iterator CBytesIter;

class Minstrel;

class Rate {
public:
	EtherAddress _eth;
	uint8_t _rate;
	uint8_t _prob;
	Rate() :
			_eth(EtherAddress()), _rate(0), _prob(0) {
	}
	Rate(EtherAddress eth, uint8_t rate, uint8_t prob) :
			_eth(eth), _rate(rate), _prob(prob) {
	}
	String unparse() {
		StringAccum sa;
		sa << _eth.unparse() << _rate << ' ' << _prob;
		return sa.take_string();
	}
};


class NetworkPort {
public:
	EtherAddress _hwaddr;
	String _iface;
	uint16_t _port_id;
	NetworkPort() :
			_hwaddr(EtherAddress()), _port_id(0) {
	}
	NetworkPort(EtherAddress hwaddr, String iface, uint16_t port_id) :
			_hwaddr(hwaddr), _iface(iface), _port_id(port_id) {
	}
	String unparse() {
		StringAccum sa;
		sa << _hwaddr.unparse() << ' ' << _iface << ' ' << _port_id;
		return sa.take_string();
	}
};

// An EmPOWER Virtual Access Point or VAP. This is an AP than
// can be used by multiple clients (unlike the LVAP that is
// specific to each client.
// The _net_bssid is write only and is used to set the bssid
// mask, meaning that the monitor interface must ACK frames
// addressed to this VAP
class EmpowerVAPState {
public:
	EtherAddress _net_bssid;
	String _ssid;
	EtherAddress _hwaddr;
	int _channel;
	int _band;
	int _iface_id;
};

// An EmPOWER Light Virtual Access Point or LVAP. This is an
// AP that is defined for a specific client. The net_bssid is
// generated from a controller-specific prefix plus the last
// three bytes of the station address. The net_bssid is write
// only and is used to set the bssid mask. Meaning that frame
// addressed to net_bssid must be acked by this WTP.
// The lvap_bssid is the one to which the client is currently
// attached. The lvap_bssid can be modified only as the result
// of auth request and assoc request messages. Probe request
// cannot modify the lvap_bssid.
class EmpowerStationState {
public:
	EtherAddress _sta;
	EtherAddress _net_bssid;
	EtherAddress _lvap_bssid;
	EtherAddress _encap;
	Vector<String> _ssids;
	String _ssid;
	int _assoc_id;
	EtherAddress _hwaddr;
	int _channel;
	empower_bands_types _band;
	int _iface_id;
	bool _set_mask;
	bool _authentication_status;
	bool _association_status;
	CBytes _rx;
	CBytes _tx;
	void update_tx(uint16_t len) {
		if (_tx.find(len) == _tx.end()) {
			_tx.set(len, 0);
		}
		(*_tx.get_pointer(len))++;
	}
	void update_rx(uint16_t len) {
		if (_rx.find(len) == _rx.end()) {
			_rx.set(len, 0);
		}
		(*_rx.get_pointer(len))++;
	}
};

// Cross structure mapping bssids to list of associated
// station and to the interface id
class EmpowerInfoBssid {
public:
	EtherAddress _bssid;
	Vector<EtherAddress> _stas;
	int _iface_id;
};

typedef HashTable<EtherAddress, EmpowerInfoBssid> InfoBssid;
typedef InfoBssid::iterator IBIter;

typedef HashTable<EtherAddress, EmpowerVAPState> VAP;
typedef VAP::iterator VAPIter;

typedef HashTable<EtherAddress, EmpowerStationState> LVAP;
typedef LVAP::iterator LVAPIter;

typedef HashTable<int, NetworkPort> Ports;
typedef Ports::iterator PortsIter;

class ResourceElement {
public:

	EtherAddress _hwaddr;
	int _channel;
	empower_bands_types _band;

	ResourceElement() :
			_hwaddr(EtherAddress()), _channel(1), _band(EMPOWER_BT_L20) {
	}

	ResourceElement(EtherAddress hwaddr, int channel, empower_bands_types band) :
			_hwaddr(hwaddr), _channel(channel), _band(band) {
	}

	inline size_t hashcode() const {
		return _channel | _band;
	}	

	inline String unparse() const {
		StringAccum sa;
		sa << "(";
		sa << _hwaddr.unparse();
		sa << ", ";
		sa << _channel;
		sa << ", ";
		switch (_band) {
		case EMPOWER_BT_L20:
			sa << "L20";
			break;
		case EMPOWER_BT_HT20:
			sa << "HT20";
			break;
		case EMPOWER_BT_HT40:
			sa << "HT40";
			break;
		}
		sa << ")";
		return sa.take_string();
	}

};

inline bool operator==(const ResourceElement &a, const ResourceElement &b) {
	return a._hwaddr == b._hwaddr && a._channel == b._channel && a._band == b._band;
}

typedef HashTable<ResourceElement, int> IfTable;
typedef IfTable::const_iterator IfIter;

typedef HashTable<int, ResourceElement> RETable;
typedef RETable::const_iterator REIter;

class EmpowerLVAPManager: public Element {
public:

	EmpowerLVAPManager();
	~EmpowerLVAPManager();

	const char *class_name() const { return "EmpowerLVAPManager"; }
	const char *port_count() const { return "1/1"; }
	const char *processing() const { return PUSH; }

	int initialize(ErrorHandler *);
	int configure(Vector<String> &, ErrorHandler *);
	void add_handlers();
	void run_timer(Timer *);
	void reset();

	void push(int, Packet *);

	int handle_add_lvap(Packet *, uint32_t);
	int handle_del_lvap(Packet *, uint32_t);
	int handle_add_vap(Packet *, uint32_t);
	int handle_del_vap(Packet *, uint32_t);
	int handle_probe_response(Packet *, uint32_t);
	int handle_auth_response(Packet *, uint32_t);
	int handle_assoc_response(Packet *, uint32_t);
	int handle_counters_request(Packet *, uint32_t);
	int handle_caps_request(Packet *, uint32_t);
	int handle_add_rssi_trigger(Packet *, uint32_t);
	int handle_del_rssi_trigger(Packet *, uint32_t);
	int handle_del_summary_trigger(Packet *, uint32_t);
	int handle_add_summary_trigger(Packet *, uint32_t);
	int handle_uimg_request(Packet *, uint32_t);
	int handle_nimg_request(Packet *, uint32_t);
	int handle_set_port(Packet *, uint32_t);
	int handle_frames_request(Packet *, uint32_t);
	int handle_link_stats_request(Packet *, uint32_t);
	int handle_set_channel(Packet *, uint32_t);
	// handle scan request
	int handle_scan_request(Packet *, uint32_t);

	void send_hello();
	void send_probe_request(EtherAddress, String, uint8_t);
	void send_auth_request(EtherAddress, EtherAddress);
	void send_association_request(EtherAddress, EtherAddress, String);
	void send_status_lvap(EtherAddress);
	void send_status_vap(EtherAddress);

	void send_status_port(EtherAddress, int);
	void send_status_port(EtherAddress, EtherAddress, int, empower_bands_types);
	void send_status_port(EtherAddress, int, EtherAddress, int, empower_bands_types);

	void send_counters_response(EtherAddress, uint32_t);
	void send_img_response(NeighborTable *, int, EtherAddress, uint32_t, EtherAddress, uint8_t, empower_bands_types);
	void send_caps_response();
	void send_set_channel_response();
	void send_rssi_trigger(EtherAddress, uint32_t, uint8_t, uint8_t, uint8_t);
	void send_summary_trigger(SummaryTrigger *);
	void send_summary(EtherAddress, uint32_t, const Vector<Frame> &);
	void send_link_stats_response(EtherAddress, uint32_t);
	// send scan results
	void send_scan_response(EtherAddress, uint32_t);
	
	EtherAddress wtp() { return _wtp; }
	EtherAddress empower_hwaddr() { return _empower_hwaddr; }
	LVAP* lvaps() { return &_lvaps; }
	VAP* vaps() { return &_vaps; }

	InfoBssid* info_bssids() {
		InfoBssid * _info_bssids = new InfoBssid();
		for (LVAPIter it = _lvaps.begin(); it.live(); it++) {
			if (!it.value()._set_mask) {
				continue;
			}
			if (!it.value()._authentication_status) {
				continue;
			}
			if (!it.value()._association_status) {
				continue;
			}
			if (_info_bssids->find(it.value()._lvap_bssid) == _info_bssids->end()) {
				EmpowerInfoBssid info;
				info._bssid = it.value()._lvap_bssid;
				info._iface_id = it.value()._iface_id;
				_info_bssids->set(it.value()._lvap_bssid, info);
			}
			EmpowerInfoBssid *eib = _info_bssids->get_pointer(it.value()._lvap_bssid);
			eib->_stas.push_back(it.value()._sta);
		}
		return _info_bssids;
	}

	Ports* ports() { return &_ports; }
	uint32_t get_next_seq() { return ++_seq; }

	const IfTable & elements() { return _elements_to_ifaces; }

	int element_to_iface(EtherAddress hwaddr, uint8_t channel, empower_bands_types band) {
		IfIter iter = _elements_to_ifaces.find(ResourceElement(hwaddr, channel, band));
		if (iter == _elements_to_ifaces.end()) {
			return -1;
		}
		return iter.value();
	}

	ResourceElement* iface_to_element(int iface) {
		return _ifaces_to_elements.get_pointer(iface);
	}

	Vector<Minstrel *> * rcs() { return &_rcs; }

private:

	IfTable _elements_to_ifaces;
	RETable _ifaces_to_elements;

	void compute_bssid_mask();

	class Empower11k *_e11k;
	class EmpowerBeaconSource *_ebs;
	class EmpowerOpenAuthResponder *_eauthr;
	class EmpowerAssociationResponder *_eassor;
	class EmpowerDeAuthResponder *_edeauthr;
	class EmpowerRXStats *_ers;
	class Counter *_uplink;
	class Counter *_downlink;

	String _empower_iface;
	EtherAddress _empower_hwaddr;
	LVAP _lvaps;
	Ports _ports;
	VAP _vaps;
	Vector<EtherAddress> _masks;
	Vector<Minstrel *> _rcs;
	Vector<String> _debugfs_strings;
	Timer _timer;
	uint32_t _seq;
	EtherAddress _wtp;
	unsigned int _period; // msecs
	bool _debug;

	static int write_handler(const String &, Element *, void *, ErrorHandler *);
	static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif
