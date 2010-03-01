/* $%BEGINLICENSE%$
 Copyright (C) 2010 Sun Microsystems, Inc

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 $%ENDLICENSE%$ */

#ifndef __CHASSIS_GTIMEVAL_H__
#define __CHASSIS_GTIMEVAL_H__

#include <glib.h>
#include "glib-ext.h"

/**
 * stores the current time in the location passed as argument
 * if time is seen to move backwards, output an error message and
 * set the time to "0".
 * @param pointer to a GTimeVal struct
 * @return difference in usec between provided timestamp and "now"
 */

CHASSIS_API gint64 chassis_gtime_testset_now(GTimeVal *gt);
#endif