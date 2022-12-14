/* Copyright (C) 2018, 2020 D. R. Commander.  All Rights Reserved.
 * Copyright (C) 2002-2005 RealVNC Ltd.  All Rights Reserved.
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

package com.turbovnc.rfb;

import com.turbovnc.rdr.*;

public class HextileDecoder extends Decoder {

  public HextileDecoder(CMsgReader reader_) { reader = reader_; }

  public void readRect(Rect r, CMsgHandler handler) {
    InStream is = reader.getInStream();
    int bytesPerPixel = handler.cp.pf().bpp / 8;
    boolean bigEndian = handler.cp.pf().bigEndian;

    int[] buf = reader.getImageBuf(16 * 16 * 4);

    Rect t = new Rect();
    int bg = 0;
    int fg = 0;

    for (t.tl.y = r.tl.y; t.tl.y < r.br.y; t.tl.y += 16) {

      t.br.y = Math.min(r.br.y, t.tl.y + 16);

      for (t.tl.x = r.tl.x; t.tl.x < r.br.x; t.tl.x += 16) {

        t.br.x = Math.min(r.br.x, t.tl.x + 16);

        int tileType = is.readU8();

        if ((tileType & RFB.HEXTILE_RAW) != 0) {
          is.readPixels(buf, t.area(), bytesPerPixel, bigEndian, true);
          handler.imageRect(t, buf);
          continue;
        }

        if ((tileType & RFB.HEXTILE_BACKGROUND_SPECIFIED) != 0)
          bg = is.readPixel(bytesPerPixel, bigEndian);

        int len = t.area();
        int ptr = 0;
        while (len-- > 0) buf[ptr++] = bg;

        if ((tileType & RFB.HEXTILE_FOREGROUND_SPECIFIED) != 0)
          fg = is.readPixel(bytesPerPixel, bigEndian);

        if ((tileType & RFB.HEXTILE_ANY_SUBRECTS) != 0) {
          int nSubrects = is.readU8();

          for (int i = 0; i < nSubrects; i++) {

            if ((tileType & RFB.HEXTILE_SUBRECTS_COLOURED) != 0)
              fg = is.readPixel(bytesPerPixel, bigEndian);

            int xy = is.readU8();
            int wh = is.readU8();

            int x = ((xy >> 4) & 15);
            int y = (xy & 15);
            int w = ((wh >> 4) & 15) + 1;
            int h = (wh & 15) + 1;
            ptr = y * t.width() + x;
            int rowAdd = t.width() - w;
            while (h-- > 0) {
              len = w;
              while (len-- > 0) buf[ptr++] = fg;
              ptr += rowAdd;
            }
          }
        }
        handler.imageRect(t, buf);
      }
    }
  }

  CMsgReader reader;
}
