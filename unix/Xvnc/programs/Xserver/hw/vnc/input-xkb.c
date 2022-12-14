/* Copyright (C) 2014-2015, 2017 D. R. Commander
 * Copyright 2013 Pierre Ossman for Cendio AB
 * Copyright (C) 2009 Red Hat, Inc.
 * Copyright (C) 2009 TightVNC Team
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
 * USA.
 */

/*

Copyright 1985, 1987, 1994, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.

*/

#ifdef HAVE_DIX_CONFIG_H
#include <dix-config.h>
#endif

#include <stdio.h>
#include <X11/X.h>
#include <X11/Xdefs.h>
#include <X11/Xfuncproto.h>
#include <X11/keysym.h>
#include "xkbsrv.h"
#include "xkbstr.h"
#include "scrnintstr.h"
#include "mi.h"
#include "input-xkb.h"
#include "rfb.h"


extern DeviceIntPtr kbdDevice;


/* From libX11 1.5.0 */

static Bool XkbTranslateKeyCode(register XkbDescPtr xkb, KeyCode key,
                                register unsigned int mods,
                                unsigned int *mods_rtrn, KeySym *keysym_rtrn)
{
  XkbKeyTypeRec *type;
  int col, nKeyGroups;
  unsigned preserve, effectiveGroup;
  KeySym *syms;

  if (mods_rtrn != NULL)
    *mods_rtrn = 0;

  nKeyGroups = XkbKeyNumGroups(xkb, key);
  if ((!XkbKeycodeInRange(xkb, key)) || (nKeyGroups == 0)) {
    if (keysym_rtrn != NULL)
      *keysym_rtrn = NoSymbol;
    return FALSE;
  }

  syms = XkbKeySymsPtr(xkb, key);

  /* find the offset of the effective group */
  col = 0;
  effectiveGroup = XkbGroupForCoreState(mods);
  if (effectiveGroup >= nKeyGroups) {
    unsigned groupInfo = XkbKeyGroupInfo(xkb, key);
    switch (XkbOutOfRangeGroupAction(groupInfo)) {
      default:
        effectiveGroup %= nKeyGroups;
        break;
      case XkbClampIntoRange:
        effectiveGroup = nKeyGroups - 1;
        break;
      case XkbRedirectIntoRange:
        effectiveGroup = XkbOutOfRangeGroupNumber(groupInfo);
        if (effectiveGroup >= nKeyGroups)
          effectiveGroup = 0;
        break;
    }
  }
  col = effectiveGroup * XkbKeyGroupsWidth(xkb, key);
  type = XkbKeyKeyType(xkb, key, effectiveGroup);

  preserve = 0;
  if (type->map) {  /* find the column (shift level) within the group */
    register int i;
    register XkbKTMapEntryPtr entry;
    for (i = 0, entry = type->map; i < type->map_count; i++, entry++) {
      if ((entry->active) && ((mods & type->mods.mask) == entry->mods.mask)) {
        col += entry->level;
        if (type->preserve)
          preserve = type->preserve[i].mask;
        break;
      }
    }
  }

  if (keysym_rtrn != NULL)
    *keysym_rtrn = syms[col];
  if (mods_rtrn)
    *mods_rtrn = type->mods.mask & (~preserve);

  return syms[col] != NoSymbol;
}


static XkbAction *XkbKeyActionPtr(XkbDescPtr xkb, KeyCode key,
                                  unsigned int mods)
{
  XkbKeyTypeRec *type;
  int col, nKeyGroups;
  unsigned effectiveGroup;
  XkbAction *acts;

  if (!XkbKeyHasActions(xkb, key))
    return NULL;

  nKeyGroups = XkbKeyNumGroups(xkb, key);
  if ((!XkbKeycodeInRange(xkb, key)) || (nKeyGroups == 0))
    return NULL;

  acts = XkbKeyActionsPtr(xkb, key);

  /* find the offset of the effective group */
  col = 0;
  effectiveGroup = XkbGroupForCoreState(mods);
  if (effectiveGroup >= nKeyGroups) {
    unsigned groupInfo = XkbKeyGroupInfo(xkb, key);
    switch (XkbOutOfRangeGroupAction(groupInfo)) {
      default:
        effectiveGroup %= nKeyGroups;
        break;
      case XkbClampIntoRange:
        effectiveGroup = nKeyGroups - 1;
        break;
      case XkbRedirectIntoRange:
        effectiveGroup = XkbOutOfRangeGroupNumber(groupInfo);
        if (effectiveGroup >= nKeyGroups)
          effectiveGroup = 0;
        break;
    }
  }
  col = effectiveGroup * XkbKeyGroupsWidth(xkb, key);
  type = XkbKeyKeyType(xkb, key, effectiveGroup);

  if (type->map) {  /* find the column (shift level) within the group */
    register int i;
    register XkbKTMapEntryPtr entry;
    for (i = 0, entry = type->map; i < type->map_count; i++, entry++) {
      if ((entry->active) && ((mods & type->mods.mask) == entry->mods.mask)) {
        col += entry->level;
        break;
      }
    }
  }

  return &acts[col];
}


static unsigned XkbKeyEffectiveGroup(XkbDescPtr xkb, KeyCode key,
                                     unsigned int mods)
{
  int nKeyGroups;
  unsigned effectiveGroup;

  nKeyGroups = XkbKeyNumGroups(xkb, key);
  if ((!XkbKeycodeInRange(xkb, key)) || (nKeyGroups == 0))
    return 0;

  effectiveGroup = XkbGroupForCoreState(mods);
  if (effectiveGroup >= nKeyGroups) {
    unsigned groupInfo = XkbKeyGroupInfo(xkb, key);
    switch (XkbOutOfRangeGroupAction(groupInfo)) {
      default:
        effectiveGroup %= nKeyGroups;
        break;
      case XkbClampIntoRange:
        effectiveGroup = nKeyGroups - 1;
        break;
      case XkbRedirectIntoRange:
        effectiveGroup = XkbOutOfRangeGroupNumber(groupInfo);
        if (effectiveGroup >= nKeyGroups)
          effectiveGroup = 0;
        break;
    }
  }

  return effectiveGroup;
}


unsigned GetKeyboardState(void)
{
  DeviceIntPtr master;

  master = GetMaster(kbdDevice, KEYBOARD_OR_FLOAT);
  return XkbStateFieldFromRec(&master->key->xkbInfo->state);
}


unsigned GetLevelThreeMask(void)
{
  unsigned state;
  KeyCode keycode;
  XkbDescPtr xkb;
  XkbAction *act;

  /* Group state is still important */
  state = GetKeyboardState();
  state &= ~0xff;

  keycode = KeysymToKeycode(XK_ISO_Level3_Shift, state, NULL);
  if (keycode == 0) {
    keycode = KeysymToKeycode(XK_Mode_switch, state, NULL);
    if (keycode == 0)
      return 0;
  }

  xkb = GetMaster(kbdDevice, KEYBOARD_OR_FLOAT)->key->xkbInfo->desc;

  act = XkbKeyActionPtr(xkb, keycode, state);
  if (act == NULL)
    return 0;
  if (act->type != XkbSA_SetMods)
    return 0;

  if (act->mods.flags & XkbSA_UseModMapMods)
    return xkb->map->modmap[keycode];
  else
    return act->mods.mask;
}


KeyCode PressShift(void)
{
  unsigned state;

  XkbDescPtr xkb;
  unsigned int key;

  state = GetKeyboardState();
  if (state & ShiftMask)
    return 0;

  xkb = GetMaster(kbdDevice, KEYBOARD_OR_FLOAT)->key->xkbInfo->desc;
  for (key = xkb->min_key_code; key <= xkb->max_key_code; key++) {
    XkbAction *act;
    unsigned char mask;

    act = XkbKeyActionPtr(xkb, key, state);
    if (act == NULL)
      continue;

    if (act->type != XkbSA_SetMods)
      continue;

    if (act->mods.flags & XkbSA_UseModMapMods)
      mask = xkb->map->modmap[key];
    else
      mask = act->mods.mask;

    if ((mask & ShiftMask) == ShiftMask)
      return key;
  }

  return 0;
}


KeyCode *ReleaseShift(void)
{
  unsigned state;
  KeyCode *keys = NULL;
  int nKeys = 0;

  DeviceIntPtr master;
  XkbDescPtr xkb;
  unsigned int key;

  state = GetKeyboardState();
  if (!(state & ShiftMask))
    return keys;

  master = GetMaster(kbdDevice, KEYBOARD_OR_FLOAT);
  xkb = master->key->xkbInfo->desc;
  for (key = xkb->min_key_code; key <= xkb->max_key_code; key++) {
    XkbAction *act;
    unsigned char mask;

    if (!key_is_down(master, key, KEY_PROCESSED))
      continue;

    act = XkbKeyActionPtr(xkb, key, state);
    if (act == NULL)
      continue;

    if (act->type != XkbSA_SetMods)
      continue;

    if (act->mods.flags & XkbSA_UseModMapMods)
      mask = xkb->map->modmap[key];
    else
      mask = act->mods.mask;

    if (!(mask & ShiftMask))
      continue;

    nKeys++;
    keys = (KeyCode *)rfbRealloc(keys, sizeof(KeyCode) * (nKeys + 1));
    keys[nKeys - 1] = key;
    keys[nKeys] = 0;
  }

  return keys;
}


KeyCode PressLevelThree(void)
{
  unsigned state, mask;

  KeyCode keycode;
  XkbDescPtr xkb;
  XkbAction *act;

  mask = GetLevelThreeMask();
  if (mask == 0)
    return 0;

  state = GetKeyboardState();
  if (state & mask)
    return 0;

  keycode = KeysymToKeycode(XK_ISO_Level3_Shift, state, NULL);
  if (keycode == 0) {
    keycode = KeysymToKeycode(XK_Mode_switch, state, NULL);
    if (keycode == 0)
      return 0;
  }

  xkb = GetMaster(kbdDevice, KEYBOARD_OR_FLOAT)->key->xkbInfo->desc;

  act = XkbKeyActionPtr(xkb, keycode, state);
  if (act == NULL)
    return 0;
  if (act->type != XkbSA_SetMods)
    return 0;

  return keycode;
}


KeyCode *ReleaseLevelThree(void)
{
  unsigned state, mask;
  KeyCode *keys = NULL;
  int nKeys = 0;

  DeviceIntPtr master;
  XkbDescPtr xkb;
  unsigned int key;

  mask = GetLevelThreeMask();
  if (mask == 0)
    return keys;

  state = GetKeyboardState();
  if (!(state & mask))
    return keys;

  master = GetMaster(kbdDevice, KEYBOARD_OR_FLOAT);
  xkb = master->key->xkbInfo->desc;
  for (key = xkb->min_key_code; key <= xkb->max_key_code; key++) {
    XkbAction *act;
    unsigned char key_mask;

    if (!key_is_down(master, key, KEY_PROCESSED))
      continue;

    act = XkbKeyActionPtr(xkb, key, state);
    if (act == NULL)
      continue;

    if (act->type != XkbSA_SetMods)
      continue;

    if (act->mods.flags & XkbSA_UseModMapMods)
      key_mask = xkb->map->modmap[key];
    else
      key_mask = act->mods.mask;

    if (!(key_mask & mask))
      continue;

    nKeys++;
    keys = (KeyCode *)rfbRealloc(keys, sizeof(KeyCode) * (nKeys + 1));
    keys[nKeys - 1] = key;
    keys[nKeys] = 0;
  }

  return keys;
}


KeyCode KeysymToKeycode(KeySym keysym, unsigned state, unsigned *new_state)
{
  XkbDescPtr xkb;
  unsigned int key;
  KeySym ks;
  unsigned level_three_mask;

  if (new_state != NULL)
    *new_state = state;

  xkb = GetMaster(kbdDevice, KEYBOARD_OR_FLOAT)->key->xkbInfo->desc;
  for (key = xkb->min_key_code; key <= xkb->max_key_code; key++) {
    unsigned int state_out;
    KeySym dummy;

    XkbTranslateKeyCode(xkb, key, state, &state_out, &ks);
    if (ks == NoSymbol)
      continue;

    /*
     * Despite every known piece of documentation on
     * XkbTranslateKeyCode() stating that mods_rtrn returns
     * the unconsumed modifiers, in reality it always
     * returns the _potentially consumed_ modifiers.
     */
    state_out = state & ~state_out;
    if (state_out & LockMask)
      XkbConvertCase(ks, &dummy, &ks);

    if (ks == keysym)
      return key;
  }

  if (new_state == NULL)
    return 0;

  *new_state = (state & ~ShiftMask) | ((state & ShiftMask) ? 0 : ShiftMask);
  key = KeysymToKeycode(keysym, *new_state, NULL);
  if (key != 0)
    return key;

  level_three_mask = GetLevelThreeMask();
  if (level_three_mask == 0)
    return 0;

  *new_state = (state & ~level_three_mask) |
               ((state & level_three_mask) ? 0 : level_three_mask);
  key = KeysymToKeycode(keysym, *new_state, NULL);
  if (key != 0)
    return key;

  *new_state = (state & ~(ShiftMask | level_three_mask)) |
               ((state & ShiftMask) ? 0 : ShiftMask) |
               ((state & level_three_mask) ? 0 : level_three_mask);
  key = KeysymToKeycode(keysym, *new_state, NULL);
  if (key != 0)
    return key;

  return 0;
}


Bool IsLockModifier(KeyCode keycode, unsigned state)
{
  XkbDescPtr xkb;
  XkbAction *act;

  xkb = GetMaster(kbdDevice, KEYBOARD_OR_FLOAT)->key->xkbInfo->desc;

  act = XkbKeyActionPtr(xkb, keycode, state);
  if (act == NULL)
    return FALSE;

  if (act->type != XkbSA_LockMods)
    return FALSE;

  return TRUE;
}


Bool IsAffectedByNumLock(KeyCode keycode)
{
  unsigned state;

  KeyCode numlock_keycode;
  unsigned numlock_mask;

  XkbDescPtr xkb;
  XkbAction *act;

  unsigned group;
  XkbKeyTypeRec *type;

  /* Group state is still important */
  state = GetKeyboardState();
  state &= ~0xff;

  /*
   * Not sure if hunting for a virtual modifier called "NumLock",
   * or following the keysym Num_Lock is the best approach. We
   * try the latter.
   */
  numlock_keycode = KeysymToKeycode(XK_Num_Lock, state, NULL);
  if (numlock_keycode == 0)
    return FALSE;

  xkb = GetMaster(kbdDevice, KEYBOARD_OR_FLOAT)->key->xkbInfo->desc;

  act = XkbKeyActionPtr(xkb, numlock_keycode, state);
  if (act == NULL)
    return FALSE;
  if (act->type != XkbSA_LockMods)
    return FALSE;

  if (act->mods.flags & XkbSA_UseModMapMods)
    numlock_mask = xkb->map->modmap[keycode];
  else
    numlock_mask = act->mods.mask;

  group = XkbKeyEffectiveGroup(xkb, keycode, state);
  type = XkbKeyKeyType(xkb, keycode, group);
  if ((type->mods.mask & numlock_mask) == 0)
    return FALSE;

  return TRUE;
}


KeyCode AddKeysym(KeySym keysym, unsigned state)
{
  DeviceIntPtr master;
  XkbDescPtr xkb;
  unsigned int key;

  XkbEventCauseRec cause;
  XkbChangesRec changes;

  int types[1];
  KeySym *syms;
  KeySym upper, lower;

  master = GetMaster(kbdDevice, KEYBOARD_OR_FLOAT);
  xkb = master->key->xkbInfo->desc;
  for (key = xkb->max_key_code; key >= xkb->min_key_code; key--) {
    if (XkbKeyNumGroups(xkb, key) == 0)
      break;
  }

  if (key < xkb->min_key_code)
    return 0;

  memset(&changes, 0, sizeof(changes));
  memset(&cause, 0, sizeof(cause));

  XkbSetCauseUnknown(&cause);

  /*
   * Tools like xkbcomp get confused if there isn't a name
   * assigned to the keycode we're trying to use.
   */
  if (xkb->names && xkb->names->keys &&
      (xkb->names->keys[key].name[0] == '\0')) {
    xkb->names->keys[key].name[0] = 'I';
    xkb->names->keys[key].name[1] = '0' + (key / 100) % 10;
    xkb->names->keys[key].name[2] = '0' + (key /  10) % 10;
    xkb->names->keys[key].name[3] = '0' + (key /   1) % 10;

    changes.names.changed |= XkbKeyNamesMask;
    changes.names.first_key = key;
    changes.names.num_keys = 1;
  }

  /* FIXME: Verify that ONE_LEVEL/ALPHABETIC isn't screwed up */

  /*
   * For keysyms that are affected by Lock, we are better off
   * using ALPHABETIC rather than ONE_LEVEL as the latter
   * generally cannot produce lower case when Lock is active.
   */
  XkbConvertCase(keysym, &lower, &upper);
  if (upper == lower)
    types[XkbGroup1Index] = XkbOneLevelIndex;
  else
    types[XkbGroup1Index] = XkbAlphabeticIndex;

  XkbChangeTypesOfKey(xkb, key, 1, XkbGroup1Mask, types, &changes.map);

  syms = XkbKeySymsPtr(xkb, key);
  if (upper == lower)
    syms[0] = keysym;
  else {
    syms[0] = lower;
    syms[1] = upper;
  }

  changes.map.changed |= XkbKeySymsMask;
  changes.map.first_key_sym = key;
  changes.map.num_key_syms = 1;

  XkbSendNotification(master, &changes, &cause);

  return key;
}


void vncXkbProcessDeviceEvent(int screenNum, InternalEvent *event,
                              DeviceIntPtr dev)
{
  unsigned int backupctrls = 0;

  if (event->device_event.sourceid == kbdDevice->id) {
    XkbControlsPtr ctrls;

    /*
     * We need to bypass AccessX since it is timing sensitive and
     * the network can cause fake event delays.
     */
    ctrls = dev->key->xkbInfo->desc->ctrls;
    backupctrls = ctrls->enabled_ctrls;
    ctrls->enabled_ctrls &= ~XkbAllFilteredEventsMask;

    /*
     * This flag needs to be set for key repeats to be properly
     * respected.
     */
    if ((event->device_event.type == ET_KeyPress) &&
        key_is_down(dev, event->device_event.detail.key, KEY_PROCESSED))
      event->device_event.key_repeat = TRUE;
  }

  dev->public.processInputProc(event, dev);

  if (event->device_event.sourceid == kbdDevice->id) {
    XkbControlsPtr ctrls;

    ctrls = dev->key->xkbInfo->desc->ctrls;
    ctrls->enabled_ctrls = backupctrls;
  }
}
