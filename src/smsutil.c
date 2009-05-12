/*
 *
 *  oFono - Open Source Telephony
 *
 *  Copyright (C) 2008-2009  Intel Corporation. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>

#include "util.h"
#include "smsutil.h"

static inline gboolean is_bit_set(unsigned char oct, int bit)
{
	int mask = 0x1 << bit;
	return oct & mask ? TRUE : FALSE;
}

static inline unsigned char bit_field(unsigned char oct, int start, int num)
{
	unsigned char mask = (0x1 << num) - 1;

	return (oct >> start) & mask;
}

static inline int to_semi_oct(char in)
{
	int digit;

	switch (in) {
	case '0':
		digit = 0;
		break;
	case '1':
		digit = 1;
		break;
	case '2':
		digit = 2;
		break;
	case '3':
		digit = 3;
		break;
	case '4':
		digit = 4;
		break;
	case '5':
		digit = 5;
		break;
	case '6':
		digit = 6;
		break;
	case '7':
		digit = 7;
		break;
	case '8':
		digit = 8;
		break;
	case '9':
		digit = 9;
		break;
	case '*':
		digit = 10;
		break;
	case '#':
		digit = 11;
		break;
	case 'A':
	case 'a':
		digit = 12;
		break;
	case 'B':
	case 'b':
		digit = 13;
		break;
	case 'C':
	case 'c':
		digit = 14;
		break;
	default:
		digit = -1;
		break;
	}

	return digit;
}

int ud_len_in_octets(guint8 ud_len, guint8 dcs)
{
	int len_7bit = (ud_len + 1) * 7 / 8;
	int len_8bit = ud_len;
	int len_16bit = ud_len * 2;
	guint8 upper;

	if (dcs == 0)
		return len_7bit;

	upper = (dcs & 0xc0) >> 6;

	switch (upper) {
	case 0:
	case 1:
		if (dcs & 0x20) /* compressed */
			return len_8bit;

		switch ((dcs & 0x0c) >> 2) {
		case 0:
			return len_7bit;
		case 1:
			return len_8bit;
		case 2:
			return len_16bit;
		}

		return 0;
	case 2:
		return 0;
	case 3:
		switch ((dcs & 0x30) >> 4) {
		case 0:
		case 1:
			return len_7bit;
		case 2:
			return len_16bit;
		case 3:
			if (dcs & 0x4)
				return len_8bit;
			else
				return len_7bit;
		}

		break;
	default:
		break;
	};

	return 0;
}

static inline gboolean next_octet(const unsigned char *pdu, int len,
					int *offset, unsigned char *oct)
{
	if (len == *offset)
		return FALSE;

	*oct = pdu[*offset];

	*offset = *offset + 1;

	return TRUE;
}

static inline gboolean set_octet(unsigned char *pdu, int *offset,
					unsigned char oct)
{
	pdu[*offset] = oct;
	*offset = *offset + 1;

	return TRUE;
}

static gboolean encode_scts(const struct sms_scts *in, unsigned char *pdu,
				int *offset)
{
	guint timezone;

	if (in->year > 99)
		return FALSE;

	if (in->month > 12)
		return FALSE;

	if (in->day > 31)
		return FALSE;

	if (in->hour > 23)
		return FALSE;

	if (in->minute > 59)
		return FALSE;

	if (in->second > 59)
		return FALSE;

	if ((in->timezone > 12*4-1) || (in->timezone < -(12*4-1)))
		return FALSE;

	pdu = pdu + *offset;

	pdu[0] = ((in->year / 10) & 0x0f) | (((in->year % 10) & 0x0f) << 4);
	pdu[1] = ((in->month / 10) & 0x0f) | (((in->month % 10) & 0x0f) << 4);
	pdu[2] = ((in->day / 10) & 0x0f) | (((in->day % 10) & 0x0f) << 4);
	pdu[3] = ((in->hour / 10) & 0x0f) | (((in->hour % 10) & 0x0f) << 4);
	pdu[4] = ((in->minute / 10) & 0x0f) | (((in->minute % 10) & 0x0f) << 4);
	pdu[5] = ((in->second / 10) & 0x0f) | (((in->second % 10) & 0x0f) << 4);

	timezone = abs(in->timezone);

	pdu[6] = ((timezone / 10) & 0x07) | (((timezone % 10) & 0x0f) << 4);

	if (in->timezone < 0)
		pdu[6] |= 0x8;

	*offset += 7;

	return TRUE;
}

static gboolean decode_scts(const unsigned char *pdu, int len,
				int *offset, struct sms_scts *out)
{
	unsigned char oct;

	if ((len - *offset) < 7)
		return FALSE;

	next_octet(pdu, len, offset, &oct);
	out->year = (oct & 0x0f) * 10 + ((oct & 0xf0) >> 4);

	next_octet(pdu, len, offset, &oct);
	out->month = (oct & 0x0f) * 10 + ((oct & 0xf0) >> 4);

	next_octet(pdu, len, offset, &oct);
	out->day = (oct & 0x0f) * 10 + ((oct & 0xf0) >> 4);

	next_octet(pdu, len, offset, &oct);
	out->hour = (oct & 0x0f) * 10 + ((oct & 0xf0) >> 4);

	next_octet(pdu, len, offset, &oct);
	out->minute = (oct & 0x0f) * 10 + ((oct & 0xf0) >> 4);

	next_octet(pdu, len, offset, &oct);
	out->second = (oct & 0x0f) * 10 + ((oct & 0xf0) >> 4);

	next_octet(pdu, len, offset, &oct);

	/* Time Zone indicates the difference, expressed in quarters
	 * of an hour, between the local time and GMT. In the first of the two
	 * semi‑octets, the first bit (bit 3 of the seventh octet of the
	 * TP‑Service‑Centre‑Time‑Stamp field) represents the algebraic
	 * sign of this difference (0: positive, 1: negative).
	 */
	out->timezone = (oct & 0x07) * 10 + ((oct & 0xf0) >> 4);

	if (oct & 0x08)
		out->timezone = out->timezone * -1;

	return TRUE;
}

static gboolean decode_validity_period(const unsigned char *pdu, int len,
					int *offset,
					enum sms_validity_period_format vpf,
					struct sms_validity_period *vp)
{
	switch (vpf) {
	case SMS_VALIDITY_PERIOD_FORMAT_ABSENT:
		return TRUE;
	case SMS_VALIDITY_PERIOD_FORMAT_RELATIVE:
		if (!next_octet(pdu, len, offset, &vp->relative))
			return FALSE;

		return TRUE;
	case SMS_VALIDITY_PERIOD_FORMAT_ABSOLUTE:
		if (!decode_scts(pdu, len, offset, &vp->absolute))
			return FALSE;

		return TRUE;
	case SMS_VALIDITY_PERIOD_FORMAT_ENHANCED:
		/* TODO: Parse out enhanced structure properly
		 * 23.040 Section 9.2.3.12.3
		 */
		if ((len - *offset) < 7)
			return FALSE;

		memcpy(vp->enhanced, pdu + *offset, 7);

		*offset = *offset + 7;

		return TRUE;
	default:
		break;
	}

	return FALSE;
}

static gboolean encode_validity_period(const struct sms_validity_period *vp,
					enum sms_validity_period_format vpf,
					unsigned char *pdu, int *offset)
{
	switch (vpf) {
	case SMS_VALIDITY_PERIOD_FORMAT_ABSENT:
		return TRUE;
	case SMS_VALIDITY_PERIOD_FORMAT_RELATIVE:
		set_octet(pdu, offset, vp->relative);
		return TRUE;
	case SMS_VALIDITY_PERIOD_FORMAT_ABSOLUTE:
		return encode_scts(&vp->absolute, pdu, offset);
	case SMS_VALIDITY_PERIOD_FORMAT_ENHANCED:
		/* TODO: Write out proper enhanced VP structure */
		memcpy(pdu + *offset, vp->enhanced, 7);

		*offset = *offset + 7;

		return TRUE;
	default:
		break;
	}

	return FALSE;
}

static gboolean encode_address(const struct sms_address *in, gboolean sc,
				unsigned char *pdu, int *offset)
{
	size_t len = strlen(in->address);
	unsigned char addr_len = 0;
	unsigned char p[10];

	pdu = pdu + *offset;

	if (len == 0 && sc) {
		pdu[0] = 0;
		*offset = *offset + 1;

		return TRUE;
	}

	if (len == 0)
		goto out;

	if (in->number_type == SMS_NUMBER_TYPE_ALPHANUMERIC) {
		long written;
		long packed;
		unsigned char *gsm;
		unsigned char *r;

		if (len > 11)
			return FALSE;

		gsm = convert_utf8_to_gsm(in->address, len, NULL, &written, 0);

		if (!gsm)
			return FALSE;

		r = pack_7bit_own_buf(gsm, written, 0, FALSE, &packed, 0, p);

		g_free(gsm);

		if (r == NULL)
			return FALSE;

		if (sc)
			addr_len = packed + 1;
		else
			addr_len = (written * 7 + 3) / 4;
	} else {
		int j = 0;
		int i;
		int c;

		if (len > 20)
			return FALSE;

		for (i = 0; in->address[i]; i++) {
			c = to_semi_oct(in->address[i]);

			if (c < 0)
				return FALSE;

			if ((i % 2) == 0) {
				p[j] = c;
			} else {
				p[j] |= c << 4;
				j++;
			}
		}

		if ((i % 2) == 1) {
			p[j] |= 0xf0;
			j++;
		}

		if (sc)
			addr_len = j + 1;
		else
			addr_len = i;
	}

out:
	pdu[0] = addr_len;
	pdu[1] = (in->number_type << 4) | in->numbering_plan | 0x80;
	memcpy(pdu+2, p, (sc ? addr_len - 1 : (addr_len + 1) / 2));

	*offset = *offset + 2 + (sc ? addr_len - 1 : (addr_len + 1) / 2);

	return TRUE;
}

static gboolean decode_address(const unsigned char *pdu, int len,
				int *offset, gboolean sc,
				struct sms_address *out)
{
	static const char digit_lut[] = "0123456789*#abc\0";
	unsigned char addr_len;
	unsigned char addr_type;
	int byte_len;
	int i;

	if (!next_octet(pdu, len, offset, &addr_len))
		return FALSE;

	if (sc && addr_len == 0) {
		out->address[0] = '\0';
		return TRUE;
	}

	if (!next_octet(pdu, len, offset, &addr_type))
		return FALSE;

	if (sc)
		byte_len = addr_len - 1;
	else
		byte_len = (addr_len + 1) / 2;

	if ((len - *offset) < byte_len)
		return FALSE;

	out->number_type = bit_field(addr_type, 4, 3);
	out->numbering_plan = bit_field(addr_type, 0, 4);

	if (out->number_type != SMS_NUMBER_TYPE_ALPHANUMERIC) {
		unsigned char oct;

		for (i = 0; i < byte_len; i++) {
			next_octet(pdu, len, offset, &oct);

			out->address[i*2] = digit_lut[oct & 0x0f];
			out->address[i*2+1] = digit_lut[(oct & 0xf0) >> 4];
		}

		out->address[i*2] = '\0';
	} else {
		int chars;
		long written;
		unsigned char *res;
		char *utf8;

		if (sc)
			chars = byte_len * 8 / 7;
		else
			chars = addr_len * 4 / 7;

		/* This cannot happen according to 24.011, however
		 * nothing is said in 23.040
		 */
		if (chars == 0) {
			out->address[0] = '\0';
			return TRUE;
		}

		res = unpack_7bit(pdu + *offset, byte_len, 0, FALSE, chars,
					&written, 0);

		*offset = *offset + (addr_len + 1) / 2;

		if (!res)
			return FALSE;

		utf8 = convert_gsm_to_utf8(res, written, NULL, NULL, 0);

		g_free(res);

		if (!utf8)
			return FALSE;

		if (strlen(utf8) > 20) {
			g_free(utf8);
			return FALSE;
		}

		strcpy(out->address, utf8);

		g_free(utf8);
	}

	return TRUE;
}

static gboolean encode_deliver(const struct sms_deliver *in, unsigned char *pdu,
				int *offset)
{
	int ud_oct_len;
	unsigned char oct;

	oct = 0;

	if (!in->mms)
		oct |= 1 << 2;

	if (in->sri)
		oct |= 1 << 5;

	if (in->rp)
		oct |= 1 << 7;

	if (in->udhi)
		oct |= 1 << 6;

	set_octet(pdu, offset, oct);

	if (encode_address(&in->oaddr, FALSE, pdu, offset) == FALSE)
		return FALSE;

	set_octet(pdu, offset, in->pid);
	set_octet(pdu, offset, in->dcs);

	if (encode_scts(&in->scts, pdu, offset) == FALSE)
		return FALSE;

	set_octet(pdu, offset, in->udl);

	ud_oct_len = ud_len_in_octets(in->udl, in->dcs);

	memcpy(pdu + *offset, in->ud, ud_oct_len);

	*offset = *offset + ud_oct_len;

	return TRUE;
}

static gboolean decode_deliver(const unsigned char *pdu, int len,
				struct sms *out)
{
	int offset = 0;
	int expected;
	unsigned char octet;

	out->type = SMS_TYPE_DELIVER;

	if (!next_octet(pdu, len, &offset, &octet))
		return FALSE;

	out->deliver.mms = !is_bit_set(octet, 2);
	out->deliver.sri = is_bit_set(octet, 5);
	out->deliver.udhi = is_bit_set(octet, 6);
	out->deliver.rp = is_bit_set(octet, 7);

	if (!decode_address(pdu, len, &offset, FALSE, &out->deliver.oaddr))
		return FALSE;

	if (!next_octet(pdu, len, &offset, &out->deliver.pid))
		return FALSE;

	if (!next_octet(pdu, len, &offset, &out->deliver.dcs))
		return FALSE;

	if (!decode_scts(pdu, len, &offset, &out->deliver.scts))
		return FALSE;

	if (!next_octet(pdu, len, &offset, &out->deliver.udl))
		return FALSE;

	expected = ud_len_in_octets(out->deliver.udl, out->deliver.dcs);

	if ((len - offset) < expected)
		return FALSE;

	memcpy(out->deliver.ud, pdu+offset, expected);

	return TRUE;
}

static gboolean encode_submit_ack_report(const struct sms_submit_ack_report *in,
						unsigned char *pdu, int *offset)
{
	unsigned char oct;

	oct = 1;

	if (in->udhi)
		oct |= 1 << 6;

	set_octet(pdu, offset, oct);

	set_octet(pdu, offset, in->pi);

	if (!encode_scts(&in->scts, pdu, offset))
		return FALSE;

	if (in->pi & 0x1)
		set_octet(pdu, offset, in->pid);

	if (in->pi & 0x2)
		set_octet(pdu, offset, in->dcs);

	if (in->pi & 0x4) {
		int ud_oct_len = ud_len_in_octets(in->udl, in->dcs);

		set_octet(pdu, offset, in->udl);
		memcpy(pdu + *offset, in->ud, ud_oct_len);
		*offset = *offset + ud_oct_len;
	}

	return TRUE;
}

static gboolean encode_submit_err_report(const struct sms_submit_err_report *in,
						unsigned char *pdu, int *offset)
{
	unsigned char oct;

	oct = 0x1;

	if (in->udhi)
		oct |= 1 << 6;

	set_octet(pdu, offset, oct);

	set_octet(pdu, offset, in->fcs);

	set_octet(pdu, offset, in->pi);

	if (!encode_scts(&in->scts, pdu, offset))
		return FALSE;

	if (in->pi & 0x1)
		set_octet(pdu, offset, in->pid);

	if (in->pi & 0x2)
		set_octet(pdu, offset, in->dcs);

	if (in->pi & 0x4) {
		int ud_oct_len = ud_len_in_octets(in->udl, in->dcs);

		set_octet(pdu, offset, in->udl);
		memcpy(pdu + *offset, in->ud, ud_oct_len);
		*offset = *offset + ud_oct_len;
	}

	return TRUE;
}

static gboolean decode_submit_report(const unsigned char *pdu, int len,
					struct sms *out)
{
	int offset = 0;
	unsigned char octet;
	gboolean udhi;
	guint8 fcs;
	guint8 pi;
	struct sms_scts *scts;
	guint8 pid = 0;
	guint8 dcs = 0;
	guint8 udl = 0;

	if (!next_octet(pdu, len, &offset, &octet))
		return FALSE;

	udhi = is_bit_set(octet, 6);

	if (!next_octet(pdu, len, &offset, &octet))
		return FALSE;

	/* At this point we don't know whether this is an ACK or an ERROR.
	 * FCS can only have values 0x80 and above, as 0x00 - 0x7F are reserved
	 * according to 3GPP 23.040.  For PI, the values can be only in
	 * bit 0, 1, 2 with the 7th bit reserved as an extension.  Since
	 * bits 3-6 are not used, assume no extension is feasible, so if the
	 * value of this octet is >= 0x80, this is an FCS and thus an error
	 * report tpdu.
	 */

	if (octet >= 0x80) {
		out->type = SMS_TYPE_SUBMIT_REPORT_ERROR;
		fcs = octet;

		if (!next_octet(pdu, len, &offset, &octet))
			return FALSE;

		scts = &out->submit_err_report.scts;
	} else {
		scts = &out->submit_ack_report.scts;
		out->type = SMS_TYPE_SUBMIT_REPORT_ACK;
	}

	pi = octet & 0x07;

	if (!decode_scts(pdu, len, &offset, scts))
		return FALSE;

	if (pi & 0x01) {
		if (!next_octet(pdu, len, &offset, &pid))
			return FALSE;
	}

	if (pi & 0x02) {
		if (!next_octet(pdu, len, &offset, &dcs))
			return FALSE;
	}

	if (out->type == SMS_TYPE_SUBMIT_REPORT_ERROR) {
		out->submit_err_report.udhi = udhi;
		out->submit_err_report.fcs = fcs;
		out->submit_err_report.pi = pi;
		out->submit_err_report.pid = pid;
		out->submit_err_report.dcs = dcs;
	} else {
		out->submit_ack_report.udhi = udhi;
		out->submit_ack_report.pi = pi;
		out->submit_ack_report.pid = pid;
		out->submit_ack_report.dcs = dcs;
	}

	if (pi & 0x04) {
		int expected;

		if (!next_octet(pdu, len, &offset, &udl))
			return FALSE;

		expected = ud_len_in_octets(udl, dcs);

		if ((len - offset) < expected)
			return FALSE;

		if (out->type == SMS_TYPE_SUBMIT_REPORT_ERROR) {
			out->submit_err_report.udl = udl;
			memcpy(out->submit_err_report.ud,
					pdu+offset, expected);
		} else {
			out->submit_ack_report.udl = udl;
			memcpy(out->submit_ack_report.ud,
					pdu+offset, expected);
		}
	}

	return TRUE;
}

static gboolean encode_status_report(const struct sms_status_report *in,
					unsigned char *pdu, int *offset)
{
	unsigned char octet;

	octet = 0x2;

	if (!in->mms)
		octet |= 1 << 2;

	if (!in->srq)
		octet |= 1 << 5;

	if (!in->udhi)
		octet |= 1 << 6;

	set_octet(pdu, offset, octet);

	set_octet(pdu, offset, in->mr);

	if (!encode_address(&in->raddr, FALSE, pdu, offset))
		return FALSE;

	if (!encode_scts(&in->scts, pdu, offset))
		return FALSE;

	if (!encode_scts(&in->dt, pdu, offset))
		return FALSE;

	octet = in->st;
	set_octet(pdu, offset, octet);

	if (in->pi == 0)
		return TRUE;

	set_octet(pdu, offset, in->pi);

	if (in->pi & 0x01)
		set_octet(pdu, offset, in->pid);

	if (in->pi & 0x02)
		set_octet(pdu, offset, in->dcs);

	if (in->pi & 0x4) {
		int ud_oct_len = ud_len_in_octets(in->udl, in->dcs);

		set_octet(pdu, offset, in->udl);
		memcpy(pdu + *offset, in->ud, ud_oct_len);
		*offset = *offset + ud_oct_len;
	}

	return TRUE;
}

static gboolean decode_status_report(const unsigned char *pdu, int len,
					struct sms *out)
{
	int offset = 0;
	unsigned char octet;

	out->type = SMS_TYPE_STATUS_REPORT;

	if (!next_octet(pdu, len, &offset, &octet))
		return FALSE;

	out->status_report.mms = !is_bit_set(octet, 2);
	out->status_report.srq = is_bit_set(octet, 5);
	out->status_report.udhi = is_bit_set(octet, 6);

	if (!next_octet(pdu, len, &offset, &out->status_report.mr))
		return FALSE;

	if (!decode_address(pdu, len, &offset, FALSE,
				&out->status_report.raddr))
		return FALSE;

	if (!decode_scts(pdu, len, &offset, &out->status_report.scts))
		return FALSE;

	if (!decode_scts(pdu, len, &offset, &out->status_report.dt))
		return FALSE;

	if (!next_octet(pdu, len, &offset, &octet))
		return FALSE;

	out->status_report.st = octet;

	/* We have to be careful here, PI is labeled as Optional in 23.040
	 * which is different from RP-ERR & RP-ACK for both Deliver & Submit
	 * reports
	 */

	if ((len - offset) == 0)
		return TRUE;

	if (!next_octet(pdu, len, &offset, &octet))
		return FALSE;

	out->status_report.pi = octet & 0x07;

	if (out->status_report.pi & 0x01) {
		if (!next_octet(pdu, len, &offset, &out->status_report.pid))
			return FALSE;
	}

	if (out->status_report.pi & 0x02) {
		if (!next_octet(pdu, len, &offset, &out->status_report.dcs))
			return FALSE;
	} else
		out->status_report.dcs = 0;

	if (out->status_report.pi & 0x04) {
		int expected;

		if (!next_octet(pdu, len, &offset, &out->status_report.udl))
			return FALSE;

		expected = ud_len_in_octets(out->status_report.udl,
						out->status_report.dcs);

		if ((len - offset) < expected)
			return FALSE;

		memcpy(out->status_report.ud, pdu+offset, expected);
	}

	return TRUE;
}

static gboolean encode_deliver_ack_report(const struct sms_deliver_ack_report *in,
						unsigned char *pdu,
						int *offset)
{
	unsigned char oct;

	oct = 0;

	if (in->udhi)
		oct |= 1 << 6;

	set_octet(pdu, offset, oct);

	set_octet(pdu, offset, in->pi);

	if (in->pi & 0x1)
		set_octet(pdu, offset, in->pid);

	if (in->pi & 0x2)
		set_octet(pdu, offset, in->dcs);

	if (in->pi & 0x4) {
		int ud_oct_len = ud_len_in_octets(in->udl, in->dcs);

		set_octet(pdu, offset, in->udl);
		memcpy(pdu + *offset, in->ud, ud_oct_len);
		*offset = *offset + ud_oct_len;
	}

	return TRUE;
}

static gboolean encode_deliver_err_report(const struct sms_deliver_err_report *in,
						unsigned char *pdu,
						int *offset)
{
	unsigned char oct;

	oct = 0;

	if (in->udhi)
		oct |= 1 << 6;

	set_octet(pdu, offset, oct);

	set_octet(pdu, offset, in->fcs);

	set_octet(pdu, offset, in->pi);

	if (in->pi & 0x1)
		set_octet(pdu, offset, in->pid);

	if (in->pi & 0x2)
		set_octet(pdu, offset, in->dcs);

	if (in->pi & 0x4) {
		int ud_oct_len = ud_len_in_octets(in->udl, in->dcs);

		set_octet(pdu, offset, in->udl);
		memcpy(pdu + *offset, in->ud, ud_oct_len);
		*offset = *offset + ud_oct_len;
	}

	return TRUE;
}

static gboolean decode_deliver_report(const unsigned char *pdu, int len,
					struct sms *out)
{
	int offset = 0;
	unsigned char octet;
	gboolean udhi;
	guint8 fcs;
	guint8 pi;
	guint8 pid = 0;
	guint8 dcs = 0;
	guint8 udl = 0;

	if (!next_octet(pdu, len, &offset, &octet))
		return FALSE;

	udhi = is_bit_set(octet, 6);

	if (!next_octet(pdu, len, &offset, &octet))
		return FALSE;

	/* At this point we don't know whether this is an ACK or an ERROR.
	 * FCS can only have values 0x80 and above, as 0x00 - 0x7F are reserved
	 * according to 3GPP 23.040.  For PI, the values can be only in
	 * bit 0, 1, 2 with the 7th bit reserved as an extension.  Since
	 * bits 3-6 are not used, assume no extension is feasible, so if the
	 * value of this octet is >= 0x80, this is an FCS and thus an error
	 * report tpdu.
	 */

	if (octet >= 0x80) {
		out->type = SMS_TYPE_DELIVER_REPORT_ERROR;
		fcs = octet;

		if (!next_octet(pdu, len, &offset, &octet))
			return FALSE;
	} else
		out->type = SMS_TYPE_DELIVER_REPORT_ACK;

	pi = octet & 0x07;

	if (pi & 0x01) {
		if (!next_octet(pdu, len, &offset, &pid))
			return FALSE;
	}

	if (pi & 0x02) {
		if (!next_octet(pdu, len, &offset, &dcs))
			return FALSE;
	}

	if (out->type == SMS_TYPE_DELIVER_REPORT_ERROR) {
		out->deliver_err_report.udhi = udhi;
		out->deliver_err_report.fcs = fcs;
		out->deliver_err_report.pi = pi;
		out->deliver_err_report.pid = pid;
		out->deliver_err_report.dcs = dcs;
	} else {
		out->deliver_ack_report.udhi = udhi;
		out->deliver_ack_report.pi = pi;
		out->deliver_ack_report.pid = pid;
		out->deliver_ack_report.dcs = dcs;
	}

	if (pi & 0x04) {
		int expected;

		if (!next_octet(pdu, len, &offset, &udl))
			return FALSE;

		expected = ud_len_in_octets(udl, dcs);

		if ((len - offset) < expected)
			return FALSE;

		if (out->type == SMS_TYPE_DELIVER_REPORT_ERROR) {
			out->deliver_err_report.udl = udl;
			memcpy(out->deliver_err_report.ud,
					pdu+offset, expected);
		} else {
			out->deliver_ack_report.udl = udl;
			memcpy(out->deliver_ack_report.ud,
					pdu+offset, expected);
		}
	}

	return TRUE;
}

static gboolean encode_submit(const struct sms_submit *in,
					unsigned char *pdu, int *offset)
{
	unsigned char octet;
	int ud_oct_len;

	/* SMS Submit */
	octet = 0x1;

	if (in->rd)
		octet |= 1 << 2;

	if (in->rp)
		octet |= 1 << 7;

	octet |= in->vpf << 3;

	if (in->udhi)
		octet |= 1 << 6;

	if (in->srr)
		octet |= 1 << 5;

	set_octet(pdu, offset, octet);

	set_octet(pdu, offset, in->mr);

	if (encode_address(&in->daddr, FALSE, pdu, offset) == FALSE)
		return FALSE;

	set_octet(pdu, offset, in->pid);

	set_octet(pdu, offset, in->dcs);

	if (!encode_validity_period(&in->vp, in->vpf, pdu, offset))
		return FALSE;

	set_octet(pdu, offset, in->udl);

	ud_oct_len = ud_len_in_octets(in->udl, in->dcs);

	memcpy(pdu + *offset, in->ud, ud_oct_len);

	*offset = *offset + ud_oct_len;

	return TRUE;
}

static gboolean decode_submit(const unsigned char *pdu, int len,
					struct sms *out)
{
	unsigned char octet;
	int offset = 0;
	int expected;

	out->type = SMS_TYPE_SUBMIT;

	if (!next_octet(pdu, len, &offset, &octet))
		return FALSE;

	out->submit.rd = is_bit_set(octet, 2);
	out->submit.vpf = bit_field(octet, 3, 2);
	out->submit.rp = is_bit_set(octet, 7);
	out->submit.udhi = is_bit_set(octet, 6);
	out->submit.srr = is_bit_set(octet, 5);

	if (!next_octet(pdu, len, &offset, &out->submit.mr))
		return FALSE;

	if (!decode_address(pdu, len, &offset, FALSE, &out->submit.daddr))
		return FALSE;

	if (!next_octet(pdu, len, &offset, &out->submit.pid))
		return FALSE;

	if (!next_octet(pdu, len, &offset, &out->submit.dcs))
		return FALSE;

	if (!decode_validity_period(pdu, len, &offset, out->submit.vpf,
					&out->submit.vp))
		return FALSE;

	if (!next_octet(pdu, len, &offset, &out->submit.udl))
		return FALSE;

	expected = ud_len_in_octets(out->submit.udl, out->submit.dcs);

	if ((len - offset) < expected)
		return FALSE;

	memcpy(out->submit.ud, pdu+offset, expected);

	return TRUE;
}

static gboolean encode_command(const struct sms_command *in,
					unsigned char *pdu, int *offset)
{
	unsigned char octet;

	octet = 0x2;

	if (in->udhi)
		octet |= 1 << 6;

	if (in->srr)
		octet |= 1 << 5;

	set_octet(pdu, offset, octet);

	set_octet(pdu, offset, in->mr);

	set_octet(pdu, offset, in->pid);

	octet = in->ct;
	set_octet(pdu, offset, octet);

	set_octet(pdu, offset, in->mn);

	if (!encode_address(&in->daddr, FALSE, pdu, offset))
		return FALSE;

	set_octet(pdu, offset, in->cdl);

	memcpy(pdu + *offset, in->cd, in->cdl);

	*offset = *offset + in->cdl;

	return TRUE;
}

static gboolean decode_command(const unsigned char *pdu, int len,
					struct sms *out)
{
	unsigned char octet;
	int offset = 0;

	out->type = SMS_TYPE_COMMAND;

	if (!next_octet(pdu, len, &offset, &octet))
		return FALSE;

	out->command.udhi = is_bit_set(octet, 6);
	out->command.srr = is_bit_set(octet, 5);

	if (!next_octet(pdu, len, &offset, &out->command.mr))
		return FALSE;

	if (!next_octet(pdu, len, &offset, &out->command.pid))
		return FALSE;

	if (!next_octet(pdu, len, &offset, &octet))
		return FALSE;

	out->command.ct = octet;

	if (!next_octet(pdu, len, &offset, &out->command.mn))
		return FALSE;

	if (!decode_address(pdu, len, &offset, FALSE, &out->command.daddr))
		return FALSE;

	if (!next_octet(pdu, len, &offset, &out->command.cdl))
		return FALSE;

	if ((len - offset) < out->command.cdl)
		return FALSE;

	memcpy(out->command.cd, pdu+offset, out->command.cdl);

	return TRUE;
}

/* Buffer must be at least 164 (tpud) + 12 (SC address) bytes long */
gboolean encode_sms(const struct sms *in, int *len, int *tpdu_len,
			unsigned char *pdu)
{
	int offset = 0;
	int tpdu_start;

	if (in->type == SMS_TYPE_DELIVER || in->type == SMS_TYPE_SUBMIT)
		if (!encode_address(&in->sc_addr, TRUE, pdu, &offset))
			return FALSE;

	tpdu_start = offset;

	switch (in->type) {
	case SMS_TYPE_DELIVER:
		if (encode_deliver(&in->deliver, pdu, &offset) == FALSE)
			return FALSE;
		break;
	case SMS_TYPE_DELIVER_REPORT_ACK:
		if (!encode_deliver_ack_report(&in->deliver_ack_report, pdu,
						&offset))
			return FALSE;
		break;
	case SMS_TYPE_DELIVER_REPORT_ERROR:
		if (!encode_deliver_err_report(&in->deliver_err_report, pdu,
						&offset))
			return FALSE;
		break;
	case SMS_TYPE_STATUS_REPORT:
		if (!encode_status_report(&in->status_report, pdu, &offset))
			return FALSE;
		break;
	case SMS_TYPE_SUBMIT:
		if (!encode_submit(&in->submit, pdu, &offset))
			return FALSE;
		break;
	case SMS_TYPE_SUBMIT_REPORT_ACK:
		if (!encode_submit_ack_report(&in->submit_ack_report, pdu,
						&offset))
			return FALSE;
		break;
	case SMS_TYPE_SUBMIT_REPORT_ERROR:
		if (!encode_submit_err_report(&in->submit_err_report, pdu,
						&offset))
			return FALSE;
		break;
	case SMS_TYPE_COMMAND:
		if (!encode_command(&in->command, pdu, &offset))
			return FALSE;
		break;
	default:
		return FALSE;
	};

	if (tpdu_len)
		*tpdu_len = offset - tpdu_start;

	if (len)
		*len = offset;

	return TRUE;
}

gboolean decode_sms(const unsigned char *pdu, int len, gboolean outgoing,
			int tpdu_len, struct sms *out)
{
	unsigned char type;
	int offset = 0;

	if (!out)
		return FALSE;

	if (len == 0)
		return FALSE;

	if (tpdu_len < len) {
		if (!decode_address(pdu, len, &offset, TRUE, &out->sc_addr))
			return FALSE;
	}

	if ((len - offset) < tpdu_len)
		return FALSE;

	/* 23.040 9.2.3.1 */
	type = pdu[offset] & 0x3;

	if (outgoing)
		type |= 0x4;

	pdu = pdu + offset;

	switch (type) {
	case 0:
		return decode_deliver(pdu, tpdu_len, out);
	case 1:
		return decode_submit_report(pdu, tpdu_len, out);
	case 2:
		return decode_status_report(pdu, tpdu_len, out);
	case 3:
		/* According to 9.2.3.1, Reserved treated as deliver */
		return decode_deliver(pdu, tpdu_len, out);
	case 4:
		return decode_deliver_report(pdu, tpdu_len, out);
	case 5:
		return decode_submit(pdu, tpdu_len, out);
	case 6:
		return decode_command(pdu, tpdu_len, out);
	}

	return FALSE;
}
