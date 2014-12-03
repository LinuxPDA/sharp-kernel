/*
 *  sharp_keycode.h
 *
 *  Raw Keycode Definitions for SHARP PDA
 *
 *  Copyright (C) 2001 SHARP
 *
 * Change Log:
 * 	12-Dec-2002 Sharp Corporation
 *	14-Jul-2004 Lineo Solutions, Inc.  for Spitz
 */

#ifndef __SHARP_KEYCODE_H_INCLUDED
#define __SHARP_KEYCODE_H_INCLUDED

#define SLKEYCODE_VERSION       3

#define	SLKEY_A			1
#define	SLKEY_B			2
#define	SLKEY_C			3
#define	SLKEY_D			4
#define	SLKEY_E			5
#define	SLKEY_F			6
#define	SLKEY_G			7
#define	SLKEY_H			8
#define	SLKEY_I			9
#define	SLKEY_J			10
#define	SLKEY_K			11
#define	SLKEY_L			12
#define	SLKEY_M			13
#define	SLKEY_N			14
#define	SLKEY_O			15
#define	SLKEY_P			16
#define	SLKEY_Q			17
#define	SLKEY_R			18
#define	SLKEY_S			19
#define	SLKEY_T			20
#define	SLKEY_U			21
#define	SLKEY_V			22
#define	SLKEY_W			23
#define	SLKEY_X			24
#define	SLKEY_Y			25
#define	SLKEY_Z			26
#define	SLKEY_LSHIFT		27
#define	SLKEY_SHIFT		(SLKEY_LSHIFT)  /* alias for SLKEY_LSHIFT */
#define	SLKEY_ENTER		28
#define	SLKEY_F2		29
#define	SLKEY_2ND		30 /* SLKEY_F12 on Ver.1 */
#define	SLKEY_BACK_SPACE	31
#define	SLKEY_SYM		32 /* SLKEY_F3 on Ver.1 */
#define	SLKEY_FRONTLIGHT	33 /* SLKEY_F1 on Ver.1 */
#define	SLKEY_F9		34 /* SLKEY_CANCEL on Ver.1 */
#define	SLKEY_LEFT		35
#define	SLKEY_UP		36
#define	SLKEY_DOWN		37
#define	SLKEY_RIGHT		38
#define	SLKEY_F4		39
#define	SLKEY_HOME		40
#define	SLKEY_1			41
#define	SLKEY_2			42
#define	SLKEY_3			43
#define	SLKEY_4			44
#define	SLKEY_5			45
#define	SLKEY_6			46
#define	SLKEY_7			47
#define	SLKEY_8			48
#define	SLKEY_9			49
#define	SLKEY_0			50
#define	SLKEY_a_DIAERESIS	51
#define	SLKEY_u_DIAERESIS	52
#define	SLKEY_o_DIAERESIS	53
#define	SLKEY_A_DIAERESIS	54
#define	SLKEY_U_DIAERESIS	55
#define	SLKEY_O_DIAERESIS	56
#define	SLKEY_SSHARP		57
#define	SLKEY_MINUS		58
#define	SLKEY_PLUS		59
#define	SLKEY_CAPS_LOCK		60
#define	SLKEY_AT		61
#define	SLKEY_QUESTION		62
#define	SLKEY_COMMA		63
#define	SLKEY_PERIOD		64
#define	SLKEY_TAB		65
#define	SLKEY_F5		66
#define	SLKEY_F6		67
#define	SLKEY_F7		68
#define	SLKEY_SLASH		69
#define	SLKEY_APOSTROPHE	70
#define	SLKEY_SEMICOLON		71
#define	SLKEY_QUOTEDBL		72
#define	SLKEY_COLON		73
#define	SLKEY_NUMBERSIGN	74
#define	SLKEY_DOLLAR		75
#define	SLKEY_PERCENT		76
/* #define	SLKEY_ASCIICIRCUM	77 */
#define SLKEY_UNDERSCORE	77
#define	SLKEY_AMPERSAND		78
#define	SLKEY_ASTERISK		79
#define	SLKEY_PARENLEFT		80
#define	SLKEY_DELETE		81
#define	SLKEY_F10		82 /* SLKEY_END on Ver.1 */
#define	SLKEY_EQUAL		83
#define	SLKEY_PARENRIGHT	84
#define	SLKEY_ASCIITILDE	85
#define	SLKEY_LESS		86
#define	SLKEY_GREATER		87
#define	SLKEY_ACTIVITY		88 /* SLKEY_F8 on Ver.1 */
#define	SLKEY_CONTACTS		89 /* SLKEY_F9 on Ver.1 */
#define	SLKEY_MAIL		90 /* SLKEY_F10 on Ver.1 */
#define	SLKEY_F11		91
#define SLKEY_SPACE             92
#define SLKEY_PHONE             93
#define SLKEY_EXCLAM            94

#define	SLKEY_RSHIFT		103
#define SLKEY_LCONTROL          104
#define SLKEY_CONTROL           (SLKEY_LCONTROL) /* alias for SLKEY_LCONTROL */
#define SLKEY_RCONTROL          105
#define SLKEY_LALT              106
#define SLKEY_ALT               (SLKEY_LALT)  /* alias for SLKEY_LALT */
#define SLKEY_RALT              107
#define SLKEY_ALTGRAPH          108

#define SLKEY_OFF               109
#define SLKEY_MAIL2             110
#define SLKEY_SCREEN            111
#define SLKEY_NUMLOCK           112
#define SLKEY_PAGEUP            113
#define SLKEY_PAGEDOWN          114

#define SLKEY_PRINTSCREEN       115
#define	SLKEY_ASCIICIRCUM	116
#define SLKEY_SYNCSTART		117
#define SLKEY_CARKIT		118
#define SLKEY_REMOCON		119

#define SLKEY_RECORDER		120

// for C3
#define SLKEY_KANA			121
#define SLKEY_ZENHAN		122
#define SLKEY_EXSELECT		123
#define SLKEY_EXCANCEL		124
#define SLKEY_EXJOGUP		125
#define SLKEY_EXJOGDOWN		126

#if !defined(CONFIG_ARCH_PXA_SPITZ)
#define SLKEY_RCREL		95
#define SLKEY_RCVOLUP	96
#define SLKEY_RCVOLDWN	97
#define SLKEY_RCFF		98
#define SLKEY_RCREW		99
#define SLKEY_RCSTP		100
#define SLKEY_RCPLY		101
#else
// for Spitz
#define	SLKEY_RCPLY		0x40
#define	SLKEY_RCSTP		0x44
#define	SLKEY_RCFF		0x6C
#define SLKEY_RCVOLUP	0x76
#define	SLKEY_RCVOLDWN	0x77
#define	SLKEY_RCREW		0x74
#define	SLKEY_RCMUTE	0x78
#define SLKEY_RCREL		128


#define	SLKEY_AVPLAY	SLKEY_RCPLY
#define	SLKEY_AVSTOP	SLKEY_RCSTP
#define	SLKEY_AVFF		SLKEY_RCFF
#define	SLKEY_AVREW		SLKEY_RCREW
#define	SLKEY_AVMUTE	SLKEY_RCMUTE

#if 0
#define	SLKEY_RCPLY		0x40
#define	SLKEY_RCSTP		0x44
#define	SLKEY_RCFF		0x76
#define	SLKEY_RCVOLDWN	0x77
#define	SLKEY_RCFF		0x6C
#define	SLKEY_RCREW		0x74
#define	SLKEY_RCMUTE	0x78
#define SLKEY_RCREL		128
#endif

#endif

#define SLKEY_HINGEMOVED	102

/* these are not used on Ver.2 */
//#define	SLKEY_CONVERT		95
//#define	SLKEY_NONCONVERT	96
//#define	SLKEY_KANJI		97
//#define	SLKEY_KANA		98
//#define	SLKEY_ACCEPT		99
//#define	SLKEY_MODECHANGE	100
//#define	SLKEY_FINAL		101
//#define	SLKEY_PRINTSCREEN	102

#endif /* __SHARP_KEYCODE_H_INCLUDED */
