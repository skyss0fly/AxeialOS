#pragma once
#include <LimineServices.h>

/**
 * 
 * FrameBuffer for Early Boot Graphics and Debugging
 * 
 */

volatile struct 
limine_framebuffer_request 
EarlyLimineFrambuffer=
{

    .id = LIMINE_FRAMEBUFFER_REQUEST, 
	.revision = 0

};

