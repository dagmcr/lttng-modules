/* SPDX-License-Identifier: (GPL-2.0-only or LGPL-2.1-only)
 *
 * lttng/events-internal.h
 *
 * Copyright (C) 2010-2012 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 */

#ifndef _LTTNG_EVENTS_INTERNAL_H
#define _LTTNG_EVENTS_INTERNAL_H

#include <wrapper/compiler_attributes.h>

#include <lttng/events.h>

struct lttng_syscall_filter;
struct lttng_metadata_cache;
struct perf_event;
struct perf_event_attr;
struct lttng_kernel_ring_buffer_config;

enum lttng_enabler_format_type {
	LTTNG_ENABLER_FORMAT_STAR_GLOB,
	LTTNG_ENABLER_FORMAT_NAME,
};

enum channel_type {
	PER_CPU_CHANNEL,
	METADATA_CHANNEL,
};

/*
 * Objects in a linked-list of enablers, owned by an event.
 */
struct lttng_enabler_ref {
	struct list_head node;			/* enabler ref list */
	struct lttng_enabler *ref;		/* backward ref */
};

struct lttng_krp;				/* Kretprobe handling */

struct lttng_uprobe_handler {
	struct lttng_kernel_event_common *event;
	loff_t offset;
	struct uprobe_consumer up_consumer;
	struct list_head node;
};

struct lttng_kprobe {
	struct kprobe kp;
	char *symbol_name;
};

struct lttng_uprobe {
	struct inode *inode;
	struct list_head head;
};

enum lttng_syscall_entryexit {
	LTTNG_SYSCALL_ENTRY,
	LTTNG_SYSCALL_EXIT,
};

enum lttng_syscall_abi {
	LTTNG_SYSCALL_ABI_NATIVE,
	LTTNG_SYSCALL_ABI_COMPAT,
};

struct lttng_kernel_event_common_private {
	struct lttng_kernel_event_common *pub;		/* Public event interface */

	const struct lttng_kernel_event_desc *desc;
	/* Backward references: list of lttng_enabler_ref (ref to enablers) */
	struct list_head enablers_ref_head;
	int registered;					/* has reg'd tracepoint probe */
	uint64_t user_token;

	int has_enablers_without_filter_bytecode;
	/* list of struct lttng_kernel_bytecode_runtime, sorted by seqnum */
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

struct lttng_kernel_channel_common_private {
	struct lttng_kernel_channel_common *pub;

	struct file *file;			/* File associated to channel */
	unsigned int sys_enter_registered:1,
		sys_exit_registered:1,
		tstate:1;			/* Transient enable state */

	struct hlist_head *sc_table;		/* for syscall tracing */
	struct hlist_head *compat_sc_table;
	struct hlist_head *sc_exit_table;	/* for syscall exit tracing */
	struct hlist_head *compat_sc_exit_table;
	struct hlist_head sc_unknown;		/* for unknown syscalls */
	struct hlist_head sc_compat_unknown;
	struct hlist_head sc_exit_unknown;
	struct hlist_head compat_sc_exit_unknown;
	struct lttng_syscall_filter *sc_filter;
	int syscall_all_entry;
	int syscall_all_exit;
};

struct lttng_kernel_channel_buffer_private {
	struct lttng_kernel_channel_common_private parent;

	struct lttng_kernel_channel_buffer *pub;

	unsigned int id;			/* Channel ID */
	unsigned int free_event_id;		/* Next event ID to allocate */
	int header_type;			/* 0: unset, 1: compact, 2: large */

	enum channel_type channel_type;
	struct lttng_kernel_ctx *ctx;
	struct lttng_kernel_ring_buffer_channel *rb_chan;		/* Ring buffer channel */
	unsigned int metadata_dumped:1;
	struct list_head node;			/* Channel list in session */
	struct lttng_transport *transport;
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

struct lttng_interpreter_output;

enum lttng_kernel_bytecode_type {
	LTTNG_KERNEL_BYTECODE_TYPE_FILTER,
	LTTNG_KERNEL_BYTECODE_TYPE_CAPTURE,
};

struct lttng_kernel_bytecode_node {
	enum lttng_kernel_bytecode_type type;
	struct list_head node;
	struct lttng_enabler *enabler;
	struct {
		uint32_t len;
		uint32_t reloc_offset;
		uint64_t seqnum;
		char data[];
	} bc;
};

struct lttng_kernel_bytecode_runtime {
	/* Associated bytecode */
	enum lttng_kernel_bytecode_type type;
	struct lttng_kernel_bytecode_node *bc;
	int (*interpreter_func)(struct lttng_kernel_bytecode_runtime *kernel_bytecode,
				const char *interpreter_stack_data,
				struct lttng_kernel_probe_ctx *lttng_probe_ctx,
				void *caller_ctx);
	int link_failed;
	struct list_head node;	/* list of bytecode runtime in event */
	struct lttng_kernel_ctx *ctx;
};

/*
 * Enabler field, within whatever object is enabling an event. Target of
 * backward reference.
 */
struct lttng_enabler {
	enum lttng_enabler_format_type format_type;

	/* head list of struct lttng_kernel_bytecode_node */
	struct list_head filter_bytecode_head;

	struct lttng_kernel_abi_event event_param;
	unsigned int enabled:1;

	uint64_t user_token;		/* User-provided token. */
};

struct lttng_event_enabler {
	struct lttng_enabler base;
	struct list_head node;	/* per-session list of enablers */
	struct lttng_kernel_channel_buffer *chan;
};

struct lttng_event_notifier_enabler {
	struct lttng_enabler base;
	uint64_t error_counter_index;
	struct list_head node;	/* List of event_notifier enablers */
	struct lttng_event_notifier_group *group;

	/* head list of struct lttng_kernel_bytecode_node */
	struct list_head capture_bytecode_head;
	uint64_t num_captures;
};

struct lttng_ctx_value {
	union {
		int64_t s64;
		const char *str;
		double d;
	} u;
};

/*
 * We need to keep this perf counter field separately from struct
 * lttng_kernel_ctx_field because cpu hotplug needs fixed-location addresses.
 */
struct lttng_perf_counter_field {
#if (LTTNG_LINUX_VERSION_CODE >= LTTNG_KERNEL_VERSION(4,10,0))
	struct lttng_cpuhp_node cpuhp_prepare;
	struct lttng_cpuhp_node cpuhp_online;
#else
	struct notifier_block nb;
	int hp_enable;
#endif
	struct perf_event_attr *attr;
	struct perf_event **e;	/* per-cpu array */
	char *name;
	struct lttng_kernel_event_field *event_field;
};

struct lttng_kernel_ctx_field {
	const struct lttng_kernel_event_field *event_field;
	size_t (*get_size)(void *priv, struct lttng_kernel_probe_ctx *probe_ctx,
			size_t offset);
	void (*record)(void *priv, struct lttng_kernel_probe_ctx *probe_ctx,
			struct lttng_kernel_ring_buffer_ctx *ctx,
			struct lttng_kernel_channel_buffer *chan);
	void (*get_value)(void *priv, struct lttng_kernel_probe_ctx *probe_ctx,
			struct lttng_ctx_value *value);
	void (*destroy)(void *priv);
	void *priv;
};

struct lttng_kernel_ctx {
	struct lttng_kernel_ctx_field *fields;
	unsigned int nr_fields;
	unsigned int allocated_fields;
	size_t largest_align;	/* in bytes */
};

struct lttng_metadata_cache {
	char *data;			/* Metadata cache */
	unsigned int cache_alloc;	/* Metadata allocated size (bytes) */
	unsigned int metadata_written;	/* Number of bytes written in metadata cache */
	atomic_t producing;		/* Metadata being produced (incomplete) */
	struct kref refcount;		/* Metadata cache usage */
	struct list_head metadata_stream;	/* Metadata stream list */
	uuid_le uuid;			/* Trace session unique ID (copy) */
	struct mutex lock;		/* Produce/consume lock */
	uint64_t version;		/* Current version of the metadata */
};

struct lttng_metadata_stream {
	void *priv;			/* Ring buffer private data */
	struct lttng_metadata_cache *metadata_cache;
	unsigned int metadata_in;	/* Bytes read from the cache */
	unsigned int metadata_out;	/* Bytes consumed from stream */
	int finalized;			/* Has channel been finalized */
	wait_queue_head_t read_wait;	/* Reader buffer-level wait queue */
	struct list_head list;		/* Stream list */
	struct lttng_transport *transport;
	uint64_t version;		/* Current version of the metadata cache */
	bool coherent;			/* Stream in a coherent state */
};

struct lttng_kernel_channel_buffer_ops_private {
	struct lttng_kernel_channel_buffer_ops *pub;	/* Public channel buffer ops interface */

	struct lttng_kernel_ring_buffer_channel *(*channel_create)(const char *name,
				void *priv,
				void *buf_addr,
				size_t subbuf_size, size_t num_subbuf,
				unsigned int switch_timer_interval,
				unsigned int read_timer_interval);
	void (*channel_destroy)(struct lttng_kernel_ring_buffer_channel *chan);
	struct lttng_kernel_ring_buffer *(*buffer_read_open)(struct lttng_kernel_ring_buffer_channel *chan);
	int (*buffer_has_read_closed_stream)(struct lttng_kernel_ring_buffer_channel *chan);
	void (*buffer_read_close)(struct lttng_kernel_ring_buffer *buf);
	/*
	 * packet_avail_size returns the available size in the current
	 * packet. Note that the size returned is only a hint, since it
	 * may change due to concurrent writes.
	 */
	size_t (*packet_avail_size)(struct lttng_kernel_ring_buffer_channel *chan);
	wait_queue_head_t *(*get_writer_buf_wait_queue)(struct lttng_kernel_ring_buffer_channel *chan, int cpu);
	wait_queue_head_t *(*get_hp_wait_queue)(struct lttng_kernel_ring_buffer_channel *chan);
	int (*is_finalized)(struct lttng_kernel_ring_buffer_channel *chan);
	int (*is_disabled)(struct lttng_kernel_ring_buffer_channel *chan);
	int (*timestamp_begin) (const struct lttng_kernel_ring_buffer_config *config,
			struct lttng_kernel_ring_buffer *bufb,
			uint64_t *timestamp_begin);
	int (*timestamp_end) (const struct lttng_kernel_ring_buffer_config *config,
			struct lttng_kernel_ring_buffer *bufb,
			uint64_t *timestamp_end);
	int (*events_discarded) (const struct lttng_kernel_ring_buffer_config *config,
			struct lttng_kernel_ring_buffer *bufb,
			uint64_t *events_discarded);
	int (*content_size) (const struct lttng_kernel_ring_buffer_config *config,
			struct lttng_kernel_ring_buffer *bufb,
			uint64_t *content_size);
	int (*packet_size) (const struct lttng_kernel_ring_buffer_config *config,
			struct lttng_kernel_ring_buffer *bufb,
			uint64_t *packet_size);
	int (*stream_id) (const struct lttng_kernel_ring_buffer_config *config,
			struct lttng_kernel_ring_buffer *bufb,
			uint64_t *stream_id);
	int (*current_timestamp) (const struct lttng_kernel_ring_buffer_config *config,
			struct lttng_kernel_ring_buffer *bufb,
			uint64_t *ts);
	int (*sequence_number) (const struct lttng_kernel_ring_buffer_config *config,
			struct lttng_kernel_ring_buffer *bufb,
			uint64_t *seq);
	int (*instance_id) (const struct lttng_kernel_ring_buffer_config *config,
			struct lttng_kernel_ring_buffer *bufb,
			uint64_t *id);
};

struct lttng_counter_ops {
	struct lib_counter *(*counter_create)(size_t nr_dimensions,
			const size_t *max_nr_elem,	/* for each dimension */
			int64_t global_sum_step);
	void (*counter_destroy)(struct lib_counter *counter);
	int (*counter_add)(struct lib_counter *counter, const size_t *dimension_indexes,
			int64_t v);
	/*
	 * counter_read reads a specific cpu's counter if @cpu >= 0, or
	 * the global aggregation counter if @cpu == -1.
	 */
	int (*counter_read)(struct lib_counter *counter, const size_t *dimension_indexes, int cpu,
			 int64_t *value, bool *overflow, bool *underflow);
	/*
	 * counter_aggregate returns the total sum of all per-cpu counters and
	 * the global aggregation counter.
	 */
	int (*counter_aggregate)(struct lib_counter *counter, const size_t *dimension_indexes,
			int64_t *value, bool *overflow, bool *underflow);
	int (*counter_clear)(struct lib_counter *counter, const size_t *dimension_indexes);
};

struct lttng_counter {
	struct file *file;		/* File associated to counter. */
	struct file *owner;
	struct lttng_counter_transport *transport;
	struct lib_counter *counter;
	struct lttng_counter_ops *ops;
};

#define LTTNG_EVENT_NOTIFIER_HT_BITS		12
#define LTTNG_EVENT_NOTIFIER_HT_SIZE		(1U << LTTNG_EVENT_NOTIFIER_HT_BITS)

struct lttng_event_notifier_ht {
	struct hlist_head table[LTTNG_EVENT_NOTIFIER_HT_SIZE];
};

struct lttng_event_notifier_group {
	struct file *file;		/* File associated to event notifier group */
	struct file *notif_file;	/* File used to expose notifications to userspace. */
	struct list_head node;		/* event notifier group list */
	struct list_head enablers_head; /* List of enablers */
	struct list_head event_notifiers_head; /* List of event notifier */
	struct lttng_event_notifier_ht event_notifiers_ht; /* Hash table of event notifiers */
	struct lttng_kernel_channel_buffer_ops *ops;
	struct lttng_transport *transport;
	struct lttng_kernel_ring_buffer_channel *chan;		/* Ring buffer channel for event notifier group. */
	struct lttng_kernel_ring_buffer *buf;	/* Ring buffer for event notifier group. */
	wait_queue_head_t read_wait;
	struct irq_work wakeup_pending;	/* Pending wakeup irq work. */
	struct lttng_kernel_event_notifier *sc_unknown;	/* for unknown syscalls */
	struct lttng_kernel_event_notifier *sc_compat_unknown;

	struct lttng_syscall_filter *sc_filter;

	struct hlist_head *event_notifier_syscall_dispatch;
	struct hlist_head *event_notifier_compat_syscall_dispatch;
	struct hlist_head *event_notifier_exit_syscall_dispatch;
	struct hlist_head *event_notifier_exit_compat_syscall_dispatch;

	struct hlist_head event_notifier_unknown_syscall_dispatch;
	struct hlist_head event_notifier_compat_unknown_syscall_dispatch;
	struct hlist_head event_notifier_exit_unknown_syscall_dispatch;
	struct hlist_head event_notifier_exit_compat_unknown_syscall_dispatch;

	int syscall_all_entry;
	int syscall_all_exit;

	unsigned int sys_enter_registered:1, sys_exit_registered:1;

	struct lttng_counter *error_counter;
	size_t error_counter_len;
};

struct lttng_transport {
	char *name;
	struct module *owner;
	struct list_head node;
	struct lttng_kernel_channel_buffer_ops ops;
};

struct lttng_counter_transport {
	char *name;
	struct module *owner;
	struct list_head node;
	struct lttng_counter_ops ops;
};

#define LTTNG_EVENT_HT_BITS		12
#define LTTNG_EVENT_HT_SIZE		(1U << LTTNG_EVENT_HT_BITS)

struct lttng_event_ht {
	struct hlist_head table[LTTNG_EVENT_HT_SIZE];
};

struct lttng_kernel_session_private {
	struct lttng_kernel_session *pub;	/* Public session interface */

	int been_active;			/* Has trace session been active ? */
	struct file *file;			/* File associated to session */
	struct list_head chan;			/* Channel list head */
	struct list_head events;		/* Event list head */
	struct list_head list;			/* Session list */
	unsigned int free_chan_id;		/* Next chan ID to allocate */
	uuid_le uuid;				/* Trace session unique ID */
	struct lttng_metadata_cache *metadata_cache;
	unsigned int metadata_dumped:1,
		tstate:1;			/* Transient enable state */
	/* List of event enablers */
	struct list_head enablers_head;
	/* Hash table of events */
	struct lttng_event_ht events_ht;
	char name[LTTNG_KERNEL_ABI_SESSION_NAME_LEN];
	char creation_time[LTTNG_KERNEL_ABI_SESSION_CREATION_TIME_ISO8601_LEN];
};

struct lttng_id_hash_node {
	struct hlist_node hlist;
	int id;
};

enum tracker_type {
	TRACKER_PID,
	TRACKER_VPID,
	TRACKER_UID,
	TRACKER_VUID,
	TRACKER_GID,
	TRACKER_VGID,

	TRACKER_UNKNOWN,
};

struct lttng_kernel_id_tracker_private {
	struct lttng_kernel_id_tracker *pub;	/* Public interface */

	struct lttng_kernel_session *session;
	enum tracker_type tracker_type;
};

extern struct lttng_kernel_ctx *lttng_static_ctx;

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
	case 8:
		lttng_fallthrough;
	case 16:
		lttng_fallthrough;
	case 32:
		lttng_fallthrough;
	case 64:
		break;
	default:
		return false;
	}
	return true;
}

int lttng_kernel_interpret_event_filter(const struct lttng_kernel_event_common *event,
		const char *interpreter_stack_data,
		struct lttng_kernel_probe_ctx *probe_ctx,
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

int lttng_context_init(void);
void lttng_context_exit(void);
int lttng_kernel_context_append(struct lttng_kernel_ctx **ctx_p,
		const struct lttng_kernel_ctx_field *f);
void lttng_kernel_context_remove_last(struct lttng_kernel_ctx **ctx_p);
struct lttng_kernel_ctx_field *lttng_kernel_get_context_field_from_index(struct lttng_kernel_ctx *ctx,
		size_t index);
int lttng_kernel_find_context(struct lttng_kernel_ctx *ctx, const char *name);
int lttng_kernel_get_context_index(struct lttng_kernel_ctx *ctx, const char *name);
void lttng_kernel_destroy_context(struct lttng_kernel_ctx *ctx);
int lttng_add_pid_to_ctx(struct lttng_kernel_ctx **ctx);
int lttng_add_cpu_id_to_ctx(struct lttng_kernel_ctx **ctx);
int lttng_add_procname_to_ctx(struct lttng_kernel_ctx **ctx);
int lttng_add_prio_to_ctx(struct lttng_kernel_ctx **ctx);
int lttng_add_nice_to_ctx(struct lttng_kernel_ctx **ctx);
int lttng_add_vpid_to_ctx(struct lttng_kernel_ctx **ctx);
int lttng_add_tid_to_ctx(struct lttng_kernel_ctx **ctx);
int lttng_add_vtid_to_ctx(struct lttng_kernel_ctx **ctx);
int lttng_add_ppid_to_ctx(struct lttng_kernel_ctx **ctx);
int lttng_add_vppid_to_ctx(struct lttng_kernel_ctx **ctx);
int lttng_add_hostname_to_ctx(struct lttng_kernel_ctx **ctx);
int lttng_add_interruptible_to_ctx(struct lttng_kernel_ctx **ctx);
int lttng_add_need_reschedule_to_ctx(struct lttng_kernel_ctx **ctx);
#if defined(CONFIG_PREEMPT_RT_FULL) || defined(CONFIG_PREEMPT)
int lttng_add_preemptible_to_ctx(struct lttng_kernel_ctx **ctx);
#else
static inline
int lttng_add_preemptible_to_ctx(struct lttng_kernel_ctx **ctx)
{
	return -ENOSYS;
}
#endif
#ifdef CONFIG_PREEMPT_RT_FULL
int lttng_add_migratable_to_ctx(struct lttng_kernel_ctx **ctx);
#else
static inline
int lttng_add_migratable_to_ctx(struct lttng_kernel_ctx **ctx)
{
	return -ENOSYS;
}
#endif

int lttng_add_callstack_to_ctx(struct lttng_kernel_ctx **ctx, int type);

#if defined(CONFIG_CGROUPS) && \
	((LTTNG_LINUX_VERSION_CODE >= LTTNG_KERNEL_VERSION(4,6,0)) || \
	 LTTNG_UBUNTU_KERNEL_RANGE(4,4,0,0, 4,5,0,0))
int lttng_add_cgroup_ns_to_ctx(struct lttng_kernel_ctx **ctx);
#else
static inline
int lttng_add_cgroup_ns_to_ctx(struct lttng_kernel_ctx **ctx)
{
	return -ENOSYS;
}
#endif

#if defined(CONFIG_IPC_NS) && \
	(LTTNG_LINUX_VERSION_CODE >= LTTNG_KERNEL_VERSION(3,8,0))
int lttng_add_ipc_ns_to_ctx(struct lttng_kernel_ctx **ctx);
#else
static inline
int lttng_add_ipc_ns_to_ctx(struct lttng_kernel_ctx **ctx)
{
	return -ENOSYS;
}
#endif

#if !defined(LTTNG_MNT_NS_MISSING_HEADER) && \
	(LTTNG_LINUX_VERSION_CODE >= LTTNG_KERNEL_VERSION(3,8,0))
int lttng_add_mnt_ns_to_ctx(struct lttng_kernel_ctx **ctx);
#else
static inline
int lttng_add_mnt_ns_to_ctx(struct lttng_kernel_ctx **ctx)
{
	return -ENOSYS;
}
#endif

#if defined(CONFIG_NET_NS) && \
	(LTTNG_LINUX_VERSION_CODE >= LTTNG_KERNEL_VERSION(3,8,0))
int lttng_add_net_ns_to_ctx(struct lttng_kernel_ctx **ctx);
#else
static inline
int lttng_add_net_ns_to_ctx(struct lttng_kernel_ctx **ctx)
{
	return -ENOSYS;
}
#endif

#if defined(CONFIG_PID_NS) && \
	(LTTNG_LINUX_VERSION_CODE >= LTTNG_KERNEL_VERSION(3,8,0))
int lttng_add_pid_ns_to_ctx(struct lttng_kernel_ctx **ctx);
#else
static inline
int lttng_add_pid_ns_to_ctx(struct lttng_kernel_ctx **ctx)
{
	return -ENOSYS;
}
#endif

#if defined(CONFIG_USER_NS) && \
	(LTTNG_LINUX_VERSION_CODE >= LTTNG_KERNEL_VERSION(3,8,0))
int lttng_add_user_ns_to_ctx(struct lttng_kernel_ctx **ctx);
#else
static inline
int lttng_add_user_ns_to_ctx(struct lttng_kernel_ctx **ctx)
{
	return -ENOSYS;
}
#endif

#if defined(CONFIG_UTS_NS) && \
	(LTTNG_LINUX_VERSION_CODE >= LTTNG_KERNEL_VERSION(3,8,0))
int lttng_add_uts_ns_to_ctx(struct lttng_kernel_ctx **ctx);
#else
static inline
int lttng_add_uts_ns_to_ctx(struct lttng_kernel_ctx **ctx)
{
	return -ENOSYS;
}
#endif

#if defined(CONFIG_TIME_NS) && \
	(LTTNG_LINUX_VERSION_CODE >= LTTNG_KERNEL_VERSION(5,6,0) || \
	LTTNG_RHEL_KERNEL_RANGE(4,18,0,305,0,0, 4,19,0,0,0,0))
int lttng_add_time_ns_to_ctx(struct lttng_kernel_ctx **ctx);
#else
static inline
int lttng_add_time_ns_to_ctx(struct lttng_kernel_ctx **ctx)
{
	return -ENOSYS;
}
#endif

int lttng_add_uid_to_ctx(struct lttng_kernel_ctx **ctx);
int lttng_add_euid_to_ctx(struct lttng_kernel_ctx **ctx);
int lttng_add_suid_to_ctx(struct lttng_kernel_ctx **ctx);
int lttng_add_gid_to_ctx(struct lttng_kernel_ctx **ctx);
int lttng_add_egid_to_ctx(struct lttng_kernel_ctx **ctx);
int lttng_add_sgid_to_ctx(struct lttng_kernel_ctx **ctx);
int lttng_add_vuid_to_ctx(struct lttng_kernel_ctx **ctx);
int lttng_add_veuid_to_ctx(struct lttng_kernel_ctx **ctx);
int lttng_add_vsuid_to_ctx(struct lttng_kernel_ctx **ctx);
int lttng_add_vgid_to_ctx(struct lttng_kernel_ctx **ctx);
int lttng_add_vegid_to_ctx(struct lttng_kernel_ctx **ctx);
int lttng_add_vsgid_to_ctx(struct lttng_kernel_ctx **ctx);

#if defined(CONFIG_PERF_EVENTS)
int lttng_add_perf_counter_to_ctx(uint32_t type,
				  uint64_t config,
				  const char *name,
				  struct lttng_kernel_ctx **ctx);
int lttng_cpuhp_perf_counter_online(unsigned int cpu,
		struct lttng_cpuhp_node *node);
int lttng_cpuhp_perf_counter_dead(unsigned int cpu,
		struct lttng_cpuhp_node *node);
#else
static inline
int lttng_add_perf_counter_to_ctx(uint32_t type,
				  uint64_t config,
				  const char *name,
				  struct lttng_kernel_ctx **ctx)
{
	return -ENOSYS;
}
static inline
int lttng_cpuhp_perf_counter_online(unsigned int cpu,
		struct lttng_cpuhp_node *node)
{
	return 0;
}
static inline
int lttng_cpuhp_perf_counter_dead(unsigned int cpu,
		struct lttng_cpuhp_node *node)
{
	return 0;
}
#endif

struct lttng_event_enabler *lttng_event_enabler_create(
		enum lttng_enabler_format_type format_type,
		struct lttng_kernel_abi_event *event_param,
		struct lttng_kernel_channel_buffer *chan);

int lttng_event_enabler_enable(struct lttng_event_enabler *event_enabler);
int lttng_event_enabler_disable(struct lttng_event_enabler *event_enabler);
struct lttng_event_notifier_enabler *lttng_event_notifier_enabler_create(
		struct lttng_event_notifier_group *event_notifier_group,
		enum lttng_enabler_format_type format_type,
		struct lttng_kernel_abi_event_notifier *event_notifier_param);

int lttng_event_notifier_enabler_enable(
		struct lttng_event_notifier_enabler *event_notifier_enabler);
int lttng_event_notifier_enabler_disable(
		struct lttng_event_notifier_enabler *event_notifier_enabler);

int lttng_event_enabler_attach_filter_bytecode(struct lttng_event_enabler *event_enabler,
		struct lttng_kernel_abi_filter_bytecode __user *bytecode);
int lttng_event_notifier_enabler_attach_filter_bytecode(
		struct lttng_event_notifier_enabler *event_notifier_enabler,
		struct lttng_kernel_abi_filter_bytecode __user *bytecode);
int lttng_event_notifier_enabler_attach_capture_bytecode(
		struct lttng_event_notifier_enabler *event_notifier_enabler,
		struct lttng_kernel_abi_capture_bytecode __user *bytecode);

int lttng_desc_match_enabler(const struct lttng_kernel_event_desc *desc,
		struct lttng_enabler *enabler);

void lttng_enabler_link_bytecode(const struct lttng_kernel_event_desc *event_desc,
		struct lttng_kernel_ctx *ctx,
		struct list_head *instance_bytecode_runtime_head,
		struct list_head *enabler_bytecode_runtime_head);

#if defined(CONFIG_HAVE_SYSCALL_TRACEPOINTS)
int lttng_syscalls_register_event(struct lttng_event_enabler *event_enabler);
int lttng_syscalls_unregister_channel(struct lttng_kernel_channel_buffer *chan);
int lttng_syscalls_destroy_event(struct lttng_kernel_channel_buffer *chan);
int lttng_syscall_filter_enable_event(
		struct lttng_kernel_channel_buffer *chan,
		struct lttng_kernel_event_recorder *event);
int lttng_syscall_filter_disable_event(
		struct lttng_kernel_channel_buffer *chan,
		struct lttng_kernel_event_recorder *event);

long lttng_channel_syscall_mask(struct lttng_kernel_channel_buffer *channel,
		struct lttng_kernel_abi_syscall_mask __user *usyscall_mask);

int lttng_syscalls_register_event_notifier(
		struct lttng_event_notifier_enabler *event_notifier_enabler);
int lttng_syscalls_create_matching_event_notifiers(
		struct lttng_event_notifier_enabler *event_notifier_enabler);
int lttng_syscalls_unregister_event_notifier_group(struct lttng_event_notifier_group *group);
int lttng_syscall_filter_enable_event_notifier(struct lttng_kernel_event_notifier *event_notifier);
int lttng_syscall_filter_disable_event_notifier(struct lttng_kernel_event_notifier *event_notifier);
#else
static inline int lttng_syscalls_register_event(
		struct lttng_event_enabler *event_enabler)
{
	return -ENOSYS;
}

static inline int lttng_syscalls_unregister_channel(struct lttng_kernel_channel_buffer *chan)
{
	return 0;
}

static inline int lttng_syscalls_destroy(struct lttng_kernel_channel_buffer *chan)
{
	return 0;
}

static inline int lttng_syscall_filter_enable_event(struct lttng_kernel_channel_buffer *chan,
		struct lttng_kernel_event_recorder *event);
{
	return -ENOSYS;
}

static inline int lttng_syscall_filter_disable_event(struct lttng_kernel_channel_buffer *chan,
		struct lttng_kernel_event_recorder *event);
{
	return -ENOSYS;
}

static inline long lttng_channel_syscall_mask(struct lttng_kernel_channel_buffer *channel,
		struct lttng_kernel_syscall_mask __user *usyscall_mask)
{
	return -ENOSYS;
}

static inline int lttng_syscalls_register_event_notifier(
		struct lttng_event_notifier_group *group)
{
	return -ENOSYS;
}

static inline int lttng_syscalls_unregister_event_notifier_group(
		struct lttng_event_notifier_group *group)
{
	return 0;
}

static inline int lttng_syscall_filter_enable_event_notifier(
		struct lttng_event_notifier_group *group,
		const char *name)
{
	return -ENOSYS;
}

static inline int lttng_syscall_filter_disable_event_notifier(
		struct lttng_event_notifier_group *group,
		const char *name)
{
	return -ENOSYS;
}

#endif

#ifdef CONFIG_KPROBES
int lttng_kprobes_register_event(const char *name,
		const char *symbol_name,
		uint64_t offset,
		uint64_t addr,
		struct lttng_kernel_event_recorder *event);
void lttng_kprobes_unregister_event(struct lttng_kernel_event_recorder *event);
void lttng_kprobes_destroy_event_private(struct lttng_kernel_event_recorder *event);
int lttng_kprobes_register_event_notifier(const char *symbol_name,
		uint64_t offset,
		uint64_t addr,
		struct lttng_kernel_event_notifier *event_notifier);
void lttng_kprobes_unregister_event_notifier(struct lttng_kernel_event_notifier *event_notifier);
void lttng_kprobes_destroy_event_notifier_private(struct lttng_kernel_event_notifier *event_notifier);
#else
static inline
int lttng_kprobes_register_event(const char *name,
		const char *symbol_name,
		uint64_t offset,
		uint64_t addr,
		struct lttng_kernel_event_recorder *event)
{
	return -ENOSYS;
}

static inline
void lttng_kprobes_unregister_event(struct lttng_kernel_event_recorder *event)
{
}

static inline
void lttng_kprobes_destroy_event_private(struct lttng_kernel_event_recorder *event)
{
}

static inline
int lttng_kprobes_register_event_notifier(const char *symbol_name,
		uint64_t offset,
		uint64_t addr,
		struct lttng_kernel_event_notifier *event_notifier)
{
	return -ENOSYS;
}

static inline
void lttng_kprobes_unregister_event_notifier(struct lttng_kernel_event_notifier *event_notifier)
{
}

static inline
void lttng_kprobes_destroy_event_notifier_private(struct lttng_kernel_event_notifier *event_notifier)
{
}
#endif

int lttng_event_add_callsite(struct lttng_kernel_event_common *event,
	struct lttng_kernel_abi_event_callsite __user *callsite);

#ifdef CONFIG_UPROBES
int lttng_uprobes_register_event(const char *name,
	int fd, struct lttng_kernel_event_recorder *event);
int lttng_uprobes_event_add_callsite(struct lttng_kernel_event_common *event,
	struct lttng_kernel_abi_event_callsite __user *callsite);
void lttng_uprobes_unregister_event(struct lttng_kernel_event_recorder *event);
void lttng_uprobes_destroy_event_private(struct lttng_kernel_event_recorder *event);
int lttng_uprobes_register_event_notifier(const char *name,
	int fd, struct lttng_kernel_event_notifier *event_notifier);
void lttng_uprobes_unregister_event_notifier(struct lttng_kernel_event_notifier *event_notifier);
void lttng_uprobes_destroy_event_notifier_private(struct lttng_kernel_event_notifier *event_notifier);
#else
static inline
int lttng_uprobes_register_event(const char *name,
	int fd, struct lttng_kernel_event_recorder *event)
{
	return -ENOSYS;
}

static inline
int lttng_uprobes_event_add_callsite(struct lttng_kernel_event_common *event,
	struct lttng_kernel_abi_event_callsite __user *callsite)
{
	return -ENOSYS;
}

static inline
void lttng_uprobes_unregister_event(struct lttng_kernel_event_recorder *event)
{
}

static inline
void lttng_uprobes_destroy_event_private(struct lttng_kernel_event_recorder *event)
{
}

static inline
int lttng_uprobes_register_event_notifier(const char *name,
	int fd, struct lttng_kernel_event_notifier *event_notifier)
{
	return -ENOSYS;
}

static inline
void lttng_uprobes_unregister_event_notifier(struct lttng_kernel_event_notifier *event_notifier)
{
}

static inline
void lttng_uprobes_destroy_event_notifier_private(struct lttng_kernel_event_notifier *event_notifier)
{
}
#endif

#ifdef CONFIG_KRETPROBES
int lttng_kretprobes_register(const char *name,
		const char *symbol_name,
		uint64_t offset,
		uint64_t addr,
		struct lttng_kernel_event_recorder *event_entry,
		struct lttng_kernel_event_recorder *event_exit);
void lttng_kretprobes_unregister(struct lttng_kernel_event_recorder *event);
void lttng_kretprobes_destroy_private(struct lttng_kernel_event_recorder *event);
int lttng_kretprobes_event_enable_state(struct lttng_kernel_event_common *event,
	int enable);
#else
static inline
int lttng_kretprobes_register(const char *name,
		const char *symbol_name,
		uint64_t offset,
		uint64_t addr,
		struct lttng_kernel_event_recorder *event_entry,
		struct lttng_kernel_event_recorder *event_exit)
{
	return -ENOSYS;
}

static inline
void lttng_kretprobes_unregister(struct lttng_kernel_event_recorder *event)
{
}

static inline
void lttng_kretprobes_destroy_private(struct lttng_kernel_event_recorder *event)
{
}

static inline
int lttng_kretprobes_event_enable_state(struct lttng_kernel_event_common *event,
	int enable)
{
	return -ENOSYS;
}
#endif

void lttng_lock_sessions(void);
void lttng_unlock_sessions(void);

struct list_head *lttng_get_probe_list_head(void);

int lttng_fix_pending_events(void);
int lttng_fix_pending_event_notifiers(void);
int lttng_session_active(void);
bool lttng_event_notifier_active(void);

struct lttng_kernel_session *lttng_session_create(void);
int lttng_session_enable(struct lttng_kernel_session *session);
int lttng_session_disable(struct lttng_kernel_session *session);
void lttng_session_destroy(struct lttng_kernel_session *session);
int lttng_session_metadata_regenerate(struct lttng_kernel_session *session);
int lttng_session_statedump(struct lttng_kernel_session *session);
void metadata_cache_destroy(struct kref *kref);

struct lttng_counter *lttng_kernel_counter_create(
		const char *counter_transport_name, size_t number_dimensions,
		const size_t *dimensions_sizes);
int lttng_kernel_counter_read(struct lttng_counter *counter,
		const size_t *dimension_indexes, int32_t cpu,
		int64_t *val, bool *overflow, bool *underflow);
int lttng_kernel_counter_aggregate(struct lttng_counter *counter,
		const size_t *dimension_indexes, int64_t *val,
		bool *overflow, bool *underflow);
int lttng_kernel_counter_clear(struct lttng_counter *counter,
		const size_t *dimension_indexes);
struct lttng_event_notifier_group *lttng_event_notifier_group_create(void);
int lttng_event_notifier_group_create_error_counter(
		struct file *event_notifier_group_file,
		const struct lttng_kernel_abi_counter_conf *error_counter_conf);
void lttng_event_notifier_group_destroy(
		struct lttng_event_notifier_group *event_notifier_group);

struct lttng_kernel_channel_buffer *lttng_channel_create(struct lttng_kernel_session *session,
				       const char *transport_name,
				       void *buf_addr,
				       size_t subbuf_size, size_t num_subbuf,
				       unsigned int switch_timer_interval,
				       unsigned int read_timer_interval,
				       enum channel_type channel_type);
struct lttng_kernel_channel_buffer *lttng_global_channel_create(struct lttng_kernel_session *session,
				       int overwrite, void *buf_addr,
				       size_t subbuf_size, size_t num_subbuf,
				       unsigned int switch_timer_interval,
				       unsigned int read_timer_interval);

void lttng_metadata_channel_destroy(struct lttng_kernel_channel_buffer *chan);
struct lttng_kernel_event_recorder *lttng_kernel_event_recorder_create(struct lttng_kernel_channel_buffer *chan,
				struct lttng_kernel_abi_event *event_param,
				const struct lttng_kernel_event_desc *event_desc,
				enum lttng_kernel_abi_instrumentation itype);
struct lttng_kernel_event_recorder *_lttng_kernel_event_recorder_create(struct lttng_kernel_channel_buffer *chan,
				struct lttng_kernel_abi_event *event_param,
				const struct lttng_kernel_event_desc *event_desc,
				enum lttng_kernel_abi_instrumentation itype);
struct lttng_kernel_event_recorder *lttng_event_compat_old_create(struct lttng_kernel_channel_buffer *chan,
		struct lttng_kernel_abi_old_event *old_event_param,
		const struct lttng_kernel_event_desc *internal_desc);

struct lttng_kernel_event_notifier *lttng_event_notifier_create(
				const struct lttng_kernel_event_desc *event_notifier_desc,
				uint64_t id,
				uint64_t error_counter_idx,
				struct lttng_event_notifier_group *event_notifier_group,
				struct lttng_kernel_abi_event_notifier *event_notifier_param,
				enum lttng_kernel_abi_instrumentation itype);
struct lttng_kernel_event_notifier *_lttng_event_notifier_create(
				const struct lttng_kernel_event_desc *event_notifier_desc,
				uint64_t id,
				uint64_t error_counter_idx,
				struct lttng_event_notifier_group *event_notifier_group,
				struct lttng_kernel_abi_event_notifier *event_notifier_param,
				enum lttng_kernel_abi_instrumentation itype);

int lttng_channel_enable(struct lttng_kernel_channel_buffer *channel);
int lttng_channel_disable(struct lttng_kernel_channel_buffer *channel);
int lttng_event_enable(struct lttng_kernel_event_common *event);
int lttng_event_disable(struct lttng_kernel_event_common *event);

void lttng_transport_register(struct lttng_transport *transport);
void lttng_transport_unregister(struct lttng_transport *transport);

void lttng_counter_transport_register(struct lttng_counter_transport *transport);
void lttng_counter_transport_unregister(struct lttng_counter_transport *transport);

void synchronize_trace(void);
int lttng_abi_init(void);
int lttng_abi_compat_old_init(void);
void lttng_abi_exit(void);
void lttng_abi_compat_old_exit(void);

const struct lttng_kernel_event_desc *lttng_event_desc_get(const char *name);
void lttng_event_desc_put(const struct lttng_kernel_event_desc *desc);
int lttng_probes_init(void);
void lttng_probes_exit(void);

int lttng_metadata_output_channel(struct lttng_metadata_stream *stream,
		struct lttng_kernel_ring_buffer_channel *chan, bool *coherent);

int lttng_id_tracker_get_node_id(const struct lttng_id_hash_node *node);
int lttng_id_tracker_empty_set(struct lttng_kernel_id_tracker *lf);
int lttng_id_tracker_init(struct lttng_kernel_id_tracker *lf,
		struct lttng_kernel_session *session,
		enum tracker_type type);
void lttng_id_tracker_fini(struct lttng_kernel_id_tracker *lf);
void lttng_id_tracker_destroy(struct lttng_kernel_id_tracker *lf, bool rcu);
int lttng_id_tracker_add(struct lttng_kernel_id_tracker *lf, int id);
int lttng_id_tracker_del(struct lttng_kernel_id_tracker *lf, int id);

int lttng_session_track_id(struct lttng_kernel_session *session,
		enum tracker_type tracker_type, int id);
int lttng_session_untrack_id(struct lttng_kernel_session *session,
		enum tracker_type tracker_type, int id);

int lttng_session_list_tracker_ids(struct lttng_kernel_session *session,
		enum tracker_type tracker_type);

void lttng_clock_ref(void);
void lttng_clock_unref(void);

void lttng_free_event_filter_runtime(struct lttng_kernel_event_common *event);

int lttng_probes_init(void);

int lttng_logger_init(void);
void lttng_logger_exit(void);

extern int lttng_statedump_start(struct lttng_kernel_session *session);

int lttng_calibrate(struct lttng_kernel_abi_calibrate *calibrate);

extern const struct file_operations lttng_tracepoint_list_fops;
extern const struct file_operations lttng_syscall_list_fops;

#define lttng_kernel_static_ctx_field(_event_field, _get_size, _record, _get_value, _destroy, _priv)	\
	__LTTNG_COMPOUND_LITERAL(const struct lttng_kernel_ctx_field, {					\
		.event_field = (_event_field),								\
		.get_size = (_get_size),								\
		.record = (_record),									\
		.get_value = (_get_value),								\
		.destroy = (_destroy),									\
		.priv = (_priv),									\
	})

#endif /* _LTTNG_EVENTS_INTERNAL_H */
