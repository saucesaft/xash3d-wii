#! /usr/bin/env python
# encoding: utf-8
# WARNING! Do not edit! https://waf.io/book/index.html#_obtaining_the_waf_file

#!/usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2008-2018 (ita)

"""
Nasm tool (asm processing)
"""

import os
import waflib.Tools.asm # leave this
from waflib.TaskGen import feature

@feature('asm')
def apply_nasm_vars(self):
	"""provided for compatibility"""
	self.env.append_value('ASFLAGS', self.to_list(getattr(self, 'nasm_flags', [])))

def configure(conf):
	"""
	Detect nasm/yasm and set the variable *AS*
	"""
	conf.find_program(['nasm', 'yasm'], var='AS')
	conf.env.AS_TGT_F = ['-o']
	conf.env.ASLNK_TGT_F = ['-o']
	conf.load('asm')
	conf.env.ASMPATH_ST = '-I%s' + os.sep
	txt = conf.cmd_and_log(conf.env.AS + ['--version'])
	if 'yasm' in txt.lower():
		conf.env.ASM_NAME = 'yasm'
	else:
		conf.env.ASM_NAME = 'nasm'
