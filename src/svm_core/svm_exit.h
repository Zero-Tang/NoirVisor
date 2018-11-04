/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018, Zero Tang. All rights reserved.

  This file is the basic Exit Handler of SVM Driver.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /svm_core/svm_main.h
*/

#include <nvdef.h>

#define noir_svm_maximum_code1		0x100
#define noir_svm_maximum_code2		0x4

typedef void (fastcall *noir_svm_exit_handler_routine)
(
 noir_gpr_state_p gpr_state,
 void* vmcb
);

noir_svm_exit_handler_routine** svm_exit_handlers=null;