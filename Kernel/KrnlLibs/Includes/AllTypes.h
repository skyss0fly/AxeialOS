#pragma once

#ifdef __StandardLIBC__

	#include <stdalign.h>
	#include <stdarg.h>
	#include <stdbool.h>
	#include <stddef.h>
	#include <fenv.h>
	#include <float.h>
	#include <iso646.h>
	#include <stdint.h>
	#include <limits.h>
	#include <stdnoreturn.h>

#else

	#include "__aln__.h"
	#include "__args__.h"
	#include "__bool__.h"
	#include "__def__.h"
	#include "__fenv__.h"
	#include "__flt__.h"
	#include "__i646__.h"
	#include "__int__.h"
	#include "__lims__.h"
	#include "__noret__.h"
	
#endif