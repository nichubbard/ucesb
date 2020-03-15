// -*- C++ -*-

#include "spec/spec.spec"
#include "spec/land_std_vme.spec"
#include "../land_common/whiterabbit.spec"
#include "aida_data.spec"

SUBEVENT(aida_subev)
{
	ts = TIMESTAMP_WHITERABBIT(id=0x200);
	subev = AIDA();
}

EVENT
{
	aida = aida_subev(type=10, subtype=1, control=37, subcrate=0);
}

#include "mapping_aida.hh"
