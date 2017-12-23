#!/bin/sh
perl ./escape_sjis.pl < w32g_syn.c > w32g_syn_escaped.c
perl ./escape_sjis.pl < w32g_subwin.c > w32g_subwin_escaped.c
perl ./escape_sjis.pl < w32g_subwin3.c > w32g_subwin3_escaped.c
perl ./escape_sjis.pl < w32g_res.rc > w32g_res_escaped.rc

