#ifndef ICMPPINGRESPONDER_HH
#define ICMPPINGRESPONDER_HH

/*
=c

ICMPPingResponder()

=s ICMP

responds to ICMP echo requests

=d

Respond to ICMP echo requests. Incoming packets must have their IP header
annotations set. The corresponding reply is generated for any ICMP echo
request and emitted on output 0. The reply's destination IP address annotation
is set appropriately; other annotations are copied from the input packet. IP
packets other than ICMP echo requests are emitted on the second output, if
there are two outputs; otherwise, they are dropped.

=a

ICMPSendPings, ICMPError */

#include <click/element.hh>

class ICMPPingResponder : public Element { public:
  
  ICMPPingResponder();
  ~ICMPPingResponder();
  
  const char *class_name() const	{ return "ICMPPingResponder"; }
  const char *processing() const	{ return "a/ah"; }  
  ICMPPingResponder *clone() const;

  void notify_noutputs(int);
  
  Packet *simple_action(Packet *);
  
};

#endif
