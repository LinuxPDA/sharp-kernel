/* deviceinfo.h:	device-specific information
 * Copyright (C) 2001  SHARP
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* mandatory entries */
extern char *di_filever(void);		/* deviceinfo format version */
extern char *di_vender(void);		/* device vendor name */
extern char *di_product(void);		/* product name */
extern char *di_revision(void);		/* ROM version */

/* optional entries */
extern char *di_locale(void);		/* locale */
extern char *di_serial(void);		/* device individual id */
extern char *di_checksum(void);		/* ROM checksum */
extern char *di_bootstr(void);		/* boot string */
