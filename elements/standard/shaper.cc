/*
 * shaper.{cc,hh} -- element limits number of successful pulls per
 * second to a given rate (packets/s)
 * Benjie Chen, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 * Copyright (c) 2000 Mazu Networks, Inc.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "shaper.hh"
#include "confparse.hh"
#include "error.hh"
#include "glue.hh"

Shaper::Shaper()
{
  add_input();
  add_output();
}

Shaper::~Shaper()
{
}

Shaper *
Shaper::clone() const
{
  return new Shaper;
}

int
Shaper::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  int rate;
  if (cp_va_parse(conf, this, errh,
		  cpUnsigned, "max allowable rate", &rate,
		  0) < 0)
    return -1;

  _meter = rate;
  _ugap = 1000000/rate;
  _total = 0;
  _start.tv_sec = 0;
  _start.tv_usec = 0;
  return 0;
}

Packet *
Shaper::pull(int)
{
  struct timeval now, diff;
  Packet *p = 0;
  click_gettimeofday(&now);

  if (_start.tv_sec == 0)
    _start = now;
  
  timersub(&now, &_start, &diff);
  
  unsigned need = diff.tv_sec * _meter;
  need += diff.tv_usec / _ugap;
  
  if (need >= _total) {
    if ((p = input(0).pull())) {
      _total++;
      if (_total > (_meter << 5)) {
	_total = 0;
	_start = now;
      }
    }
  }

  return p;
}

EXPORT_ELEMENT(Shaper)
