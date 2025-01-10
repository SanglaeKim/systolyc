/* main.h --- 
 * 
 * Filename: main.h
 * Description: 
 * Author: Sanglae Kim
 * Maintainer: 
 * Created: Mon Jan  6 16:57:08 2025 (+0900)
 * Version: 
 * Package-Requires: ()
 * Last-Updated: 
 *           By: 
 *     Update #: 0
 * URL: 
 * Doc URL: 
 * Keywords: 
 * Compatibility: 
 * 
 */

/* Commentary: 
 * 
 * 
 * 
 */

/* Change Log:
 * 
 * 
 */

/* This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with GNU Emacs.  If not, see <https://www.gnu.org/licenses/>.
 */

/* Code: */
#ifndef ECHO_H
#define ECHO_H


#include <stdint.h>


#define RECV_BUFFER_DEPTH   (8U)
#define RECV_BUFFER_SIZE    (103040U+8U)


typedef struct _StTpuPkt{
  uintptr_t puiBase;
  uint32_t uiNumBytes;
  uint8_t *pPayload;
}StTpuPkt;

typedef struct _StTpuPktHeader{
  uint32_t uiBase;
  uint32_t uiNumBytes;

}StTpuPktHeader;

int writeToSram(struct pbuf *p);


#endif /* ECHO_H */

/* main.h ends here */
