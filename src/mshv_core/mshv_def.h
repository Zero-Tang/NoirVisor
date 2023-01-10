/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2023, Zero Tang. All rights reserved.

  This file includes definitions for MSHV-Core.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /mshv_core/mshv_def.h
*/

#include <nvdef.h>

// Microsoft Hypervisor Hypercall Return Status Definitions
#define hv_status_success										0x0000
#define hv_status_invalid_hypercall_code						0x0002
#define hv_status_invalid_hypercall_input						0x0003
#define hv_status_invalid_alignment								0x0004
#define hv_status_invalid_parameter								0x0005
#define hv_status_access_denied									0x0006
#define hv_status_invalid_partition_state						0x0007
#define hv_status_operation_denied								0x0008
#define hv_status_unknown_property								0x0009
#define hv_status_property_value_out_of_range					0x000A
#define hv_status_insufficient_memory							0x000B
#define hv_status_partition_too_deep							0x000C
#define hv_status_invalid_partition_id							0x000D
#define hv_status_invalid_vp_index								0x000E
#define hv_status_invalid_port_id								0x0011
#define hv_status_invalid_connection_id							0x0012
#define hv_status_not_acknowledged								0x0014
#define hv_status_invalid_vp_state								0x0015
#define hv_status_acknowledged									0x0016
#define hv_status_invalid_save_restore_state					0x0017
#define hv_status_invalid_synic_state							0x0018
#define hv_status_object_in_use									0x0019
#define hv_status_invalid_proximity_domain_info					0x001A
#define hv_status_no_data										0x001B
#define hv_status_inactive										0x001C
#define hv_status_no_resources									0x001D
#define hv_status_feature_unavailable							0x001E
#define hv_status_partial_packet								0x001F
#define hv_status_processor_feature_not_supported				0x0020
#define hv_status_processor_cache_line_flush_size_incompatible	0x0030
#define hv_status_insufficient_buffer							0x0033
#define hv_status_incompatible_processor						0x0037
#define hv_status_insufficient_device_domains					0x0038
#define hv_status_cpuid_feature_validation_error				0x003C
#define hv_status_cpuid_xsave_feature_validation_error			0x003D
#define hv_status_processor_startup_timeout						0x003E
#define hv_status_smx_enabled									0x003F
#define hv_status_invalid_lp_index								0x0041
#define hv_status_invalid_register_value						0x0050
#define hv_status_nx_not_detected								0x0055
#define hv_status_invalid_device_id								0x0057
#define hv_status_invalid_device_state							0x0058
#define hv_status_pending_page_requests							0x0059
#define hv_status_page_request_invalid							0x0060
#define hv_status_operation_failed								0x0071
#define hv_status_not_allowed_with_nested_virt_active			0x0072