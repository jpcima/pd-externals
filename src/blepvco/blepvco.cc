/* blepvco - minBLEP-based, hard-sync-capable LADSPA VCOs.
 *
 * Copyright (C) 2004-2005 Sean Bolton.
 * Copyright (C) 2017 Jean-Pierre Cimalando.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307, USA.
 */

#include "blepvco/blepvco.h"
#include "blepvco/minblep_tables.h"
#include <cmath>

void place_step_dd(t_float *buffer, int index, t_float phase, t_float w, t_float scale)
{
    t_float r = MINBLEP_PHASES * phase / w;
    int i = std::lrint(r - 0.5_f);
    r -= (t_float)i;
    i &= MINBLEP_PHASE_MASK;  /* extreme modulation can cause i to be out-of-range */
    /* this would be better than the above, but more expensive:
     *  while (i < 0) {
     *    i += MINBLEP_PHASES;
     *    index++;
     *  }
     */

    while (i < MINBLEP_PHASES * STEP_DD_PULSE_LENGTH) {
        buffer[index] += scale * ((t_float)step_dd_table[i].value + r * (t_float)step_dd_table[i].delta);
        i += MINBLEP_PHASES;
        index++;
    }
}

void place_slope_dd(t_float *buffer, int index, t_float phase, t_float w, t_float slope_delta)
{
    t_float r = MINBLEP_PHASES * phase / w;
    int i = std::lrint(r - 0.5_f);
    r -= (t_float)i;
    i &= MINBLEP_PHASE_MASK;  /* extreme modulation can cause i to be out-of-range */

    slope_delta *= w;

    while (i < MINBLEP_PHASES * SLOPE_DD_PULSE_LENGTH) {
        buffer[index] += slope_delta * ((t_float)slope_dd_table[i] + r * (t_float)(slope_dd_table[i + 1] - slope_dd_table[i]));
        i += MINBLEP_PHASES;
        index++;
    }
}
