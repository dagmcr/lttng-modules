/* SPDX-License-Identifier: (GPL-2.0-only or LGPL-2.1-only)
 *
 * lttng/events-internal.h
 *
 * Copyright (C) 2010-2012 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 */

#ifndef _LTTNG_EVENTS_INTERNAL_H
#define _LTTNG_EVENTS_INTERNAL_H

#include <lttng/events.h>

struct lttng_kernel_event_common_private {
	struct lttng_kernel_event_common *pub;		/* Public event interface */

	const struct lttng_kernel_event_desc *desc;
	/* Backward references: list of lttng_enabler_ref (ref to enablers) */
	struct list_head enablers_ref_head;
	int registered;					/* has reg'd tracepoint probe */
	uint64_t user_token;

	int has_enablers_without_filter_bytecode;
	/* list of struct lttng_bytecode_runtime, sorted by seqnum */
	struct list_head filter_bytecode_runtime_head;
	enum lttng_kernel_abi_instrumentation instrumentation;
	/* Selected by instrumentation */
	union {
		struct lttng_kprobe kprobe;
		struct lttng_uprobe uprobe;
		struct {
			struct lttng_krp *lttng_krp;
			char *symbol_name;
		} kretprobe;
		struct {
			enum lttng_syscall_entryexit entryexit;
			enum lttng_syscall_abi abi;
			struct hlist_node node;			/* chain registered syscall event_notifier */
			unsigned int syscall_id;
		} syscall;
	} u;
};

struct lttng_kernel_event_recorder_private {
	struct lttng_kernel_event_common_private parent;

	struct lttng_kernel_event_recorder *pub;	/* Public event interface */
	struct list_head node;				/* Event recorder list */
	struct hlist_node hlist;			/* Hash table of event recorders */
	struct lttng_kernel_ctx *ctx;
	unsigned int id;
	unsigned int metadata_dumped:1;
};

struct lttng_kernel_event_notifier_private {
	struct lttng_kernel_event_common_private parent;

	struct lttng_kernel_event_notifier *pub;	/* Public event notifier interface */
	struct lttng_event_notifier_group *group;	/* weak ref */
	size_t num_captures;				/* Needed to allocate the msgpack array. */
	uint64_t error_counter_index;
	struct list_head node;				/* Event notifier list */
	struct hlist_node hlist;			/* Hash table of event notifiers */
	struct list_head capture_bytecode_runtime_head;

};

enum lttng_kernel_bytecode_interpreter_ret {
	LTTNG_KERNEL_BYTECODE_INTERPRETER_ERROR = -1,
	LTTNG_KERNEL_BYTECODE_INTERPRETER_OK = 0,
};

enum lttng_kernel_bytecode_filter_result {
	LTTNG_KERNEL_BYTECODE_FILTER_ACCEPT = 0,
	LTTNG_KERNEL_BYTECODE_FILTER_REJECT = 1,
};

struct lttng_kernel_bytecode_filter_ctx {
	enum lttng_kernel_bytecode_filter_result result;
};

/*
 * Enabler field, within whatever object is enabling an event. Target of
 * backward reference.
 */
struct lttng_enabler {
	enum lttng_enabler_format_type format_type;

	/* head list of struct lttng_bytecode_node */
	struct list_head filter_bytecode_head;

	struct lttng_kernel_abi_event event_param;
	unsigned int enabled:1;

	uint64_t user_token;		/* User-provided token. */
};

struct lttng_event_enabler {
	struct lttng_enabler base;
	struct list_head node;	/* per-session list of enablers */
	struct lttng_channel *chan;
};

struct lttng_event_notifier_enabler {
	struct lttng_enabler base;
	uint64_t error_counter_index;
	struct list_head node;	/* List of event_notifier enablers */
	struct lttng_event_notifier_group *group;

	/* head list of struct lttng_bytecode_node */
	struct list_head capture_bytecode_head;
	uint64_t num_captures;
};

static inline
const struct lttng_kernel_type_integer *lttng_kernel_get_type_integer(const struct lttng_kernel_type_common *type)
{
	if (type->type != lttng_kernel_type_integer)
		return NULL;
	return container_of(type, const struct lttng_kernel_type_integer, parent);
}

static inline
const struct lttng_kernel_type_string *lttng_kernel_get_type_string(const struct lttng_kernel_type_common *type)
{
	if (type->type != lttng_kernel_type_string)
		return NULL;
	return container_of(type, const struct lttng_kernel_type_string, parent);
}

static inline
const struct lttng_kernel_type_enum *lttng_kernel_get_type_enum(const struct lttng_kernel_type_common *type)
{
	if (type->type != lttng_kernel_type_enum)
		return NULL;
	return container_of(type, const struct lttng_kernel_type_enum, parent);
}

static inline
const struct lttng_kernel_type_array *lttng_kernel_get_type_array(const struct lttng_kernel_type_common *type)
{
	if (type->type != lttng_kernel_type_array)
		return NULL;
	return container_of(type, const struct lttng_kernel_type_array, parent);
}

static inline
const struct lttng_kernel_type_sequence *lttng_kernel_get_type_sequence(const struct lttng_kernel_type_common *type)
{
	if (type->type != lttng_kernel_type_sequence)
		return NULL;
	return container_of(type, const struct lttng_kernel_type_sequence, parent);
}

static inline
const struct lttng_kernel_type_struct *lttng_kernel_get_type_struct(const struct lttng_kernel_type_common *type)
{
	if (type->type != lttng_kernel_type_struct)
		return NULL;
	return container_of(type, const struct lttng_kernel_type_struct, parent);
}

static inline
const struct lttng_kernel_type_variant *lttng_kernel_get_type_variant(const struct lttng_kernel_type_common *type)
{
	if (type->type != lttng_kernel_type_variant)
		return NULL;
	return container_of(type, const struct lttng_kernel_type_variant, parent);
}

static inline bool lttng_kernel_type_is_bytewise_integer(const struct lttng_kernel_type_common *type)
{
	const struct lttng_kernel_type_integer *type_integer = lttng_kernel_get_type_integer(type);

	if (!type_integer)
		return false;
	switch (type_integer->size) {
	case 8:		/* Fall-through. */
	case 16:	/* Fall-through. */
	case 32:	/* Fall-through. */
	case 64:
		break;
	default:
		return false;
	}
	return true;
}

int lttng_kernel_interpret_event_filter(const struct lttng_kernel_event_common *event,
		const char *interpreter_stack_data,
		struct lttng_probe_ctx *probe_ctx,
		void *event_filter_ctx);

static inline
struct lttng_enabler *lttng_event_enabler_as_enabler(
		struct lttng_event_enabler *event_enabler)
{
	return &event_enabler->base;
}

static inline
struct lttng_enabler *lttng_event_notifier_enabler_as_enabler(
		struct lttng_event_notifier_enabler *event_notifier_enabler)
{
	return &event_notifier_enabler->base;
}

#endif /* _LTTNG_EVENTS_INTERNAL_H */
