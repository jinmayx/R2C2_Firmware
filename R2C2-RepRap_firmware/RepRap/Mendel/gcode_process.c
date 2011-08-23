/* Copyright (C) 2009-2010 Michael Moon aka Triffid_Hunter   */
/* Copyright (c) 2011 Jorge Pinto - casainho@gmail.com       */
/* All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:

   * Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
   * Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in
     the documentation and/or other materials provided with the
     distribution.
   * Neither the name of the copyright holders nor the names of
     contributors may be used to endorse or promote products derived
     from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.
*/

#include        <string.h>
#include	"gcode_process.h"
#include	"gcode_parse.h"
#include	"dda_queue.h"
#include	"serial.h"
#include	"sermsg.h"
#include	"temp.h"
#include	"sersendf.h"
#include "timer.h"
#include "pinout.h"
#include "config.h"

void zero_x(void)
{
  SpecialMoveXY(0, 0, config.maximum_feedrate_x);

  // wait for queue to complete
  for (;queue_empty() == 0;) {}

  // hit endstops, no acceleration- we don't care about skipped steps
  current_position.X = 0;
  SpecialMoveXY(-250 * config.steps_per_mm_x, 0, config.maximum_feedrate_x);
  // wait for queue to complete
  for (;queue_empty() == 0;) {}
  current_position.X = 0;

  // move forward a bit
  SpecialMoveXY(3 * config.steps_per_mm_x, 0, config.search_feedrate_x);

  // move back in to endstops slowly
  SpecialMoveXY(-6 * config.steps_per_mm_x, 0, config.search_feedrate_x);
  // wait for queue to complete
  for (;queue_empty() == 0;) {}

  // this is our home point
  startpoint.X = current_position.X = 0;
}

void zero_y(void)
{
  SpecialMoveXY(0, 0, config.maximum_feedrate_y);

  // wait for queue to complete
  for (;queue_empty() == 0;) {}

  // hit endstops, no acceleration- we don't care about skipped steps
  current_position.Y = 0;
  SpecialMoveXY(0, -250 * config.steps_per_mm_y, config.maximum_feedrate_y);
  // wait for queue to complete
  for (;queue_empty() == 0;) {}
  current_position.Y = 0;

  // move forward a bit
  SpecialMoveXY(0, 3 * config.steps_per_mm_x, config.search_feedrate_y);

  // move back in to endstops slowly
  SpecialMoveXY(0, -6 * config.steps_per_mm_y, config.search_feedrate_y);
  // wait for queue to complete
  for (;queue_empty() == 0;) {}

  // this is our home point
  startpoint.Y = current_position.Y = 0;
}

void zero_z(void)
{
  SpecialMoveXY(0, 0, config.maximum_feedrate_z);

  // wait for queue to complete
  for (;queue_empty() == 0;) {}

  // hit endstops, no acceleration- we don't care about skipped steps
  current_position.Z = 0;
  SpecialMoveZ(-250 * config.steps_per_mm_z, config.maximum_feedrate_z);
  // wait for queue to complete
  for (;queue_empty() == 0;) {}
  current_position.Z = 0;

  // move forward a bit
  SpecialMoveZ(1 * config.steps_per_mm_z, config.search_feedrate_z);

  // move back in to endstops slowly
  SpecialMoveZ(-6 * config.steps_per_mm_z, config.search_feedrate_z);
  // wait for queue to complete
  for (;queue_empty() == 0;) {}

  // this is our home point
  startpoint.Z = current_position.Z = 0;
}

void zero_e(void)
{
  // extruder only runs one way and we have no "endstop", just set this point as home
  startpoint.E = current_position.E = 0;
}

/****************************************************************************
*                                                                           *
* Command Received - process it                                             *
*                                                                           *
****************************************************************************/

void process_gcode_command()
{
  uint32_t backup_f;
  uint8_t axisSelected = 0;

  // convert relative to absolute
  if (next_target.option_relative)
  {
    next_target.target.X += startpoint.X;
    next_target.target.Y += startpoint.Y;
    next_target.target.Z += startpoint.Z;
    next_target.target.E += startpoint.E;
  }

  /* reset a target.options.g28 */
  next_target.target.options.g28 = 0;

  // E ALWAYS relative, otherwise we overflow our registers after only a few layers
  // 	next_target.target.E += startpoint.E;
  // easier way to do this
  // 	startpoint.E = 0;
  // moved to dda.c, end of dda_create() and dda_queue.c, next_move()
	
  if (next_target.seen_G)
  {
    switch (next_target.G)
    {
      // G0 - rapid, unsynchronised motion
      // since it would be a major hassle to force the dda to not synchronise, just provide a fast feedrate and hope it's close enough to what host expects
      case 0:
      backup_f = next_target.target.F;
      next_target.target.F = config.maximum_feedrate_x * 2;
      enqueue(&next_target.target);
      next_target.target.F = backup_f;
      break;

      // G1 - synchronised motion
      case 1:
      enqueue(&next_target.target);
      break;

      //	G2 - Arc Clockwise
      // unimplemented

      //	G3 - Arc Counter-clockwise
      // unimplemented

      //	G4 - Dwell
      case 4:
      // wait for all moves to complete
      for (;queue_empty() == 0;)
              //wd_reset();
      // delay
      delay_ms(next_target.P);
      break;

      //	G20 - inches as units
      case 20:
      next_target.option_inches = 1;
      break;

      //	G21 - mm as units
      case 21:
      next_target.option_inches = 0;
      break;

      //	G30 - go home via point
      case 30:
      enqueue(&next_target.target);
      // no break here, G30 is move and then go home

      //	G28 - go home
      case 28:
      if (next_target.seen_X)
      {
        zero_x();
        axisSelected = 1;
      }

      if (next_target.seen_Y)
      {
        zero_y();
        axisSelected = 1;
      }

      if (next_target.seen_Z)
      {
        zero_z();
        axisSelected = 1;
      }

      if (next_target.seen_E)
      {
        zero_e();
        axisSelected = 1;
      }

      if(!axisSelected)
      {
        zero_x();
        zero_y();
        zero_z();
        zero_e();
      }

      startpoint.F = config.search_feedrate_x;
      break;

      // G90 - absolute positioning
      case 90:
      next_target.option_relative = 0;
      break;

      // G91 - relative positioning
      case 91:
      next_target.option_relative = 1;
      break;

      //	G92 - set home
      case 92:
      // wait for queue to complete
      for (;queue_empty() == 0;) {}

      if (next_target.seen_X)
      {
        startpoint.X = current_position.X = 0;
        axisSelected = 1;
      }

      if (next_target.seen_Y)
      {
        startpoint.Y = current_position.Y = 0;
        axisSelected = 1;
      }

      if (next_target.seen_Z)
      {
        startpoint.Z = current_position.Z = 0;
        axisSelected = 1;
      }

      if (next_target.seen_E)
      {
        startpoint.E = current_position.E = 0;
        axisSelected = 1;
      }

      if(!axisSelected)
      {
          startpoint.X = current_position.X = \
          startpoint.Y = current_position.Y = \
          startpoint.Z = current_position.Z = \
          startpoint.E = current_position.E = 0;
      }
      break;

      // unknown gcode: spit an error
      default:
              serial_writestr("E: Bad G-code ");
              serwrite_uint8(next_target.G);
              serial_writechar("\r\n");
    }
  }
  else if (next_target.seen_M)
  {
    switch (next_target.M)
    {
      // M101- extruder on
      case 101:
      break;

      // M102- extruder reverse

      // M103- extruder off
      case 103:
      break;

      // M104- set temperature
      case 104:
      temp_set(next_target.S, EXTRUDER_0);
      break;

      // M105- get temperature
      case 105:
      temp_print();
      break;

      // M106- fan on
      case 106:
      break;
      // M107- fan off
      case 107:
      break;

      case 108:
      /* What is this for Skeinforge?? */
      break;

      // M109- set temp and wait
      case 109:
      temp_set(next_target.S, EXTRUDER_0);
      enqueue(NULL);
      break;

      // M110- set line number
      case 110:
      next_target.N_expected = next_target.S - 1;
      break;

      // M111- set debug level
      case 111:
      //debug_flags = next_target.S;
      break;

      // M112- immediate stop
      case 112:
      disableTimerInterrupt();
      queue_flush();
      power_off();
      break;

      // M113- extruder PWM
      case 113:
      break;

      /* M114- report XYZE to host */
      case 114:
      // wait for queue to complete
      for (;queue_empty() == 0;) {}

      if (next_target.option_inches)
      {

      }
      else
      {
        sersendf("ok C: X:%g Y:%g Z:%g E:%g\r\n", \
          (((double) current_position.X) / ((double) config.steps_per_mm_x)), \
          (((double) current_position.Y) / ((double) config.steps_per_mm_y)), \
          (((double) current_position.Z) / ((double) config.steps_per_mm_z)), \
          (((double) current_position.E) / ((double) config.steps_per_mm_e)));
      }

      break;
      
      // M115- report firmware version
		case 115:
			sersendf("FIRMWARE_NAME:Teacup_R2C2 FIRMWARE_URL:http%%3A//github.com/bitboxelectronics/R2C2 PROTOCOL_VERSION:1.0 MACHINE_TYPE:Mendel");
		break;

      // M130- heater P factor
      case 130:
      //if (next_target.seen_S)
        //p_factor = next_target.S;
      break;
              // M131- heater I factor
      case 131:
        //if (next_target.seen_S)
              //i_factor = next_target.S;
      break;

      // M132- heater D factor
      case 132:
      //if (next_target.seen_S)
              //d_factor = next_target.S;
      break;

      // M133- heater I limit
      case 133:
      //if (next_target.seen_S)
              //i_limit = next_target.S;
      break;

      // M134- save PID settings to eeprom
      case 134:
      //heater_save_settings();
      break;

      /* M140 - Bed Temperature (Fast) */
      case 140:
      temp_set(next_target.S, HEATED_BED_0);
      break;

      /* M141 - Chamber Temperature (Fast) */
      case 141:
      break;

      /* M142 - Bed Holding Pressure */
      case 142:
      break;

      // M190- power on
      case 190:
      power_on();
      x_enable();
      y_enable();
      z_enable();
      e_enable();
      steptimeout = 0;
      break;

      // M191- power off
      case 191:
      x_disable();
      y_disable();
      z_disable();
      e_disable();
      power_off();
      break;

      // unknown mcode: spit an error
      default:
      serial_writestr("E: Bad M-code ");
      serwrite_uint8(next_target.M);
      serial_writechar("\r\n");
    }
  }
}