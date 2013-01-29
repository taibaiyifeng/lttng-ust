/*
 * Copyright (C) 2011 - Julien Desfossez <julien.desfossez@polymtl.ca>
 * Copyright (C) 2011-2013 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License only.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#define _GNU_SOURCE
#include <string.h>
#include <lttng/ust-ctl.h>
#include <lttng/ust-abi.h>
#include <lttng/ust-events.h>
#include <sys/mman.h>

#include <usterr-signal-safe.h>
#include <ust-comm.h>
#include <helper.h>

#include "../libringbuffer/backend.h"
#include "../libringbuffer/frontend.h"

/*
 * Channel representation within consumer.
 */
struct ustctl_consumer_channel {
	struct lttng_channel *chan;		/* lttng channel buffers */

	/* initial attributes */
	struct ustctl_consumer_channel_attr attr;
};

/*
 * Stream representation within consumer.
 */
struct ustctl_consumer_stream {
	struct lttng_ust_shm_handle *handle;	/* shared-memory handle */
	struct lttng_ust_lib_ring_buffer *buf;
	struct ustctl_consumer_channel *chan;
	int shm_fd, wait_fd, wakeup_fd;
	int cpu;
	uint64_t memory_map_size;
};

extern void lttng_ring_buffer_client_overwrite_init(void);
extern void lttng_ring_buffer_client_discard_init(void);
extern void lttng_ring_buffer_metadata_client_init(void);
extern void lttng_ring_buffer_client_overwrite_exit(void);
extern void lttng_ring_buffer_client_discard_exit(void);
extern void lttng_ring_buffer_metadata_client_exit(void);

volatile enum ust_loglevel ust_loglevel;

int ustctl_release_handle(int sock, int handle)
{
	struct ustcomm_ust_msg lum;
	struct ustcomm_ust_reply lur;

	if (sock < 0 || handle < 0)
		return 0;
	memset(&lum, 0, sizeof(lum));
	lum.handle = handle;
	lum.cmd = LTTNG_UST_RELEASE;
	return ustcomm_send_app_cmd(sock, &lum, &lur);
}

/*
 * If sock is negative, it means we don't have to notify the other side
 * (e.g. application has already vanished).
 */
int ustctl_release_object(int sock, struct lttng_ust_object_data *data)
{
	int ret;

	if (!data)
		return -EINVAL;

	switch (data->type) {
	case LTTNG_UST_OBJECT_TYPE_CHANNEL:
		free(data->u.channel.data);
		break;
	case LTTNG_UST_OBJECT_TYPE_STREAM:
		if (data->u.stream.shm_fd >= 0) {
			ret = close(data->u.stream.shm_fd);
			if (ret < 0) {
				ret = -errno;
				return ret;
			}
		}
		if (data->u.stream.wakeup_fd >= 0) {
			ret = close(data->u.stream.wakeup_fd);
			if (ret < 0) {
				ret = -errno;
				return ret;
			}
		}
		break;
	default:
		assert(0);
	}
	return ustctl_release_handle(sock, data->handle);
}

/*
 * Send registration done packet to the application.
 */
int ustctl_register_done(int sock)
{
	struct ustcomm_ust_msg lum;
	struct ustcomm_ust_reply lur;
	int ret;

	DBG("Sending register done command to %d", sock);
	memset(&lum, 0, sizeof(lum));
	lum.handle = LTTNG_UST_ROOT_HANDLE;
	lum.cmd = LTTNG_UST_REGISTER_DONE;
	ret = ustcomm_send_app_cmd(sock, &lum, &lur);
	if (ret)
		return ret;
	return 0;
}

/*
 * returns session handle.
 */
int ustctl_create_session(int sock)
{
	struct ustcomm_ust_msg lum;
	struct ustcomm_ust_reply lur;
	int ret, session_handle;

	/* Create session */
	memset(&lum, 0, sizeof(lum));
	lum.handle = LTTNG_UST_ROOT_HANDLE;
	lum.cmd = LTTNG_UST_SESSION;
	ret = ustcomm_send_app_cmd(sock, &lum, &lur);
	if (ret)
		return ret;
	session_handle = lur.ret_val;
	DBG("received session handle %u", session_handle);
	return session_handle;
}

int ustctl_create_event(int sock, struct lttng_ust_event *ev,
		struct lttng_ust_object_data *channel_data,
		struct lttng_ust_object_data **_event_data)
{
	struct ustcomm_ust_msg lum;
	struct ustcomm_ust_reply lur;
	struct lttng_ust_object_data *event_data;
	int ret;

	if (!channel_data || !_event_data)
		return -EINVAL;

	event_data = zmalloc(sizeof(*event_data));
	if (!event_data)
		return -ENOMEM;
	memset(&lum, 0, sizeof(lum));
	lum.handle = channel_data->handle;
	lum.cmd = LTTNG_UST_EVENT;
	strncpy(lum.u.event.name, ev->name,
		LTTNG_UST_SYM_NAME_LEN);
	lum.u.event.instrumentation = ev->instrumentation;
	lum.u.event.loglevel_type = ev->loglevel_type;
	lum.u.event.loglevel = ev->loglevel;
	ret = ustcomm_send_app_cmd(sock, &lum, &lur);
	if (ret) {
		free(event_data);
		return ret;
	}
	event_data->handle = lur.ret_val;
	DBG("received event handle %u", event_data->handle);
	*_event_data = event_data;
	return 0;
}

int ustctl_add_context(int sock, struct lttng_ust_context *ctx,
		struct lttng_ust_object_data *obj_data,
		struct lttng_ust_object_data **_context_data)
{
	struct ustcomm_ust_msg lum;
	struct ustcomm_ust_reply lur;
	struct lttng_ust_object_data *context_data;
	int ret;

	if (!obj_data || !_context_data)
		return -EINVAL;

	context_data = zmalloc(sizeof(*context_data));
	if (!context_data)
		return -ENOMEM;
	memset(&lum, 0, sizeof(lum));
	lum.handle = obj_data->handle;
	lum.cmd = LTTNG_UST_CONTEXT;
	lum.u.context.ctx = ctx->ctx;
	ret = ustcomm_send_app_cmd(sock, &lum, &lur);
	if (ret) {
		free(context_data);
		return ret;
	}
	context_data->handle = lur.ret_val;
	DBG("received context handle %u", context_data->handle);
	*_context_data = context_data;
	return ret;
}

int ustctl_set_filter(int sock, struct lttng_ust_filter_bytecode *bytecode,
		struct lttng_ust_object_data *obj_data)
{
	struct ustcomm_ust_msg lum;
	struct ustcomm_ust_reply lur;
	int ret;

	if (!obj_data)
		return -EINVAL;

	memset(&lum, 0, sizeof(lum));
	lum.handle = obj_data->handle;
	lum.cmd = LTTNG_UST_FILTER;
	lum.u.filter.data_size = bytecode->len;
	lum.u.filter.reloc_offset = bytecode->reloc_offset;
	lum.u.filter.seqnum = bytecode->seqnum;

	ret = ustcomm_send_app_msg(sock, &lum);
	if (ret)
		return ret;
	/* send var len bytecode */
	ret = ustcomm_send_unix_sock(sock, bytecode->data,
				bytecode->len);
	if (ret < 0) {
		if (ret == -ECONNRESET)
			fprintf(stderr, "remote end closed connection\n");
		return ret;
	}
	if (ret != bytecode->len)
		return -EINVAL;
	return ustcomm_recv_app_reply(sock, &lur, lum.handle, lum.cmd);
}

/* Enable event, channel and session ioctl */
int ustctl_enable(int sock, struct lttng_ust_object_data *object)
{
	struct ustcomm_ust_msg lum;
	struct ustcomm_ust_reply lur;
	int ret;

	if (!object)
		return -EINVAL;

	memset(&lum, 0, sizeof(lum));
	lum.handle = object->handle;
	lum.cmd = LTTNG_UST_ENABLE;
	ret = ustcomm_send_app_cmd(sock, &lum, &lur);
	if (ret)
		return ret;
	DBG("enabled handle %u", object->handle);
	return 0;
}

/* Disable event, channel and session ioctl */
int ustctl_disable(int sock, struct lttng_ust_object_data *object)
{
	struct ustcomm_ust_msg lum;
	struct ustcomm_ust_reply lur;
	int ret;

	if (!object)
		return -EINVAL;

	memset(&lum, 0, sizeof(lum));
	lum.handle = object->handle;
	lum.cmd = LTTNG_UST_DISABLE;
	ret = ustcomm_send_app_cmd(sock, &lum, &lur);
	if (ret)
		return ret;
	DBG("disable handle %u", object->handle);
	return 0;
}

int ustctl_start_session(int sock, int handle)
{
	struct lttng_ust_object_data obj;

	obj.handle = handle;
	return ustctl_enable(sock, &obj);
}

int ustctl_stop_session(int sock, int handle)
{
	struct lttng_ust_object_data obj;

	obj.handle = handle;
	return ustctl_disable(sock, &obj);
}

int ustctl_tracepoint_list(int sock)
{
	struct ustcomm_ust_msg lum;
	struct ustcomm_ust_reply lur;
	int ret, tp_list_handle;

	memset(&lum, 0, sizeof(lum));
	lum.handle = LTTNG_UST_ROOT_HANDLE;
	lum.cmd = LTTNG_UST_TRACEPOINT_LIST;
	ret = ustcomm_send_app_cmd(sock, &lum, &lur);
	if (ret)
		return ret;
	tp_list_handle = lur.ret_val;
	DBG("received tracepoint list handle %u", tp_list_handle);
	return tp_list_handle;
}

int ustctl_tracepoint_list_get(int sock, int tp_list_handle,
		struct lttng_ust_tracepoint_iter *iter)
{
	struct ustcomm_ust_msg lum;
	struct ustcomm_ust_reply lur;
	int ret;

	if (!iter)
		return -EINVAL;

	memset(&lum, 0, sizeof(lum));
	lum.handle = tp_list_handle;
	lum.cmd = LTTNG_UST_TRACEPOINT_LIST_GET;
	ret = ustcomm_send_app_cmd(sock, &lum, &lur);
	if (ret)
		return ret;
	DBG("received tracepoint list entry name %s loglevel %d",
		lur.u.tracepoint.name,
		lur.u.tracepoint.loglevel);
	memcpy(iter, &lur.u.tracepoint, sizeof(*iter));
	return 0;
}

int ustctl_tracepoint_field_list(int sock)
{
	struct ustcomm_ust_msg lum;
	struct ustcomm_ust_reply lur;
	int ret, tp_field_list_handle;

	memset(&lum, 0, sizeof(lum));
	lum.handle = LTTNG_UST_ROOT_HANDLE;
	lum.cmd = LTTNG_UST_TRACEPOINT_FIELD_LIST;
	ret = ustcomm_send_app_cmd(sock, &lum, &lur);
	if (ret)
		return ret;
	tp_field_list_handle = lur.ret_val;
	DBG("received tracepoint field list handle %u", tp_field_list_handle);
	return tp_field_list_handle;
}

int ustctl_tracepoint_field_list_get(int sock, int tp_field_list_handle,
		struct lttng_ust_field_iter *iter)
{
	struct ustcomm_ust_msg lum;
	struct ustcomm_ust_reply lur;
	int ret;
	ssize_t len;

	if (!iter)
		return -EINVAL;

	memset(&lum, 0, sizeof(lum));
	lum.handle = tp_field_list_handle;
	lum.cmd = LTTNG_UST_TRACEPOINT_FIELD_LIST_GET;
	ret = ustcomm_send_app_cmd(sock, &lum, &lur);
	if (ret)
		return ret;
	len = ustcomm_recv_unix_sock(sock, iter, sizeof(*iter));
	if (len != sizeof(*iter)) {
		return -EINVAL;
	}
	DBG("received tracepoint field list entry event_name %s event_loglevel %d field_name %s field_type %d",
		iter->event_name,
		iter->loglevel,
		iter->field_name,
		iter->type);
	return 0;
}

int ustctl_tracer_version(int sock, struct lttng_ust_tracer_version *v)
{
	struct ustcomm_ust_msg lum;
	struct ustcomm_ust_reply lur;
	int ret;

	if (!v)
		return -EINVAL;

	memset(&lum, 0, sizeof(lum));
	lum.handle = LTTNG_UST_ROOT_HANDLE;
	lum.cmd = LTTNG_UST_TRACER_VERSION;
	ret = ustcomm_send_app_cmd(sock, &lum, &lur);
	if (ret)
		return ret;
	memcpy(v, &lur.u.version, sizeof(*v));
	DBG("received tracer version");
	return 0;
}

int ustctl_wait_quiescent(int sock)
{
	struct ustcomm_ust_msg lum;
	struct ustcomm_ust_reply lur;
	int ret;

	memset(&lum, 0, sizeof(lum));
	lum.handle = LTTNG_UST_ROOT_HANDLE;
	lum.cmd = LTTNG_UST_WAIT_QUIESCENT;
	ret = ustcomm_send_app_cmd(sock, &lum, &lur);
	if (ret)
		return ret;
	DBG("waited for quiescent state");
	return 0;
}

int ustctl_calibrate(int sock, struct lttng_ust_calibrate *calibrate)
{
	if (!calibrate)
		return -EINVAL;

	return -ENOSYS;
}

int ustctl_sock_flush_buffer(int sock, struct lttng_ust_object_data *object)
{
	struct ustcomm_ust_msg lum;
	struct ustcomm_ust_reply lur;
	int ret;

	if (!object)
		return -EINVAL;

	memset(&lum, 0, sizeof(lum));
	lum.handle = object->handle;
	lum.cmd = LTTNG_UST_FLUSH_BUFFER;
	ret = ustcomm_send_app_cmd(sock, &lum, &lur);
	if (ret)
		return ret;
	DBG("flushed buffer handle %u", object->handle);
	return 0;
}

static
int ustctl_send_channel(int sock,
		enum lttng_ust_chan_type type,
		void *data,
		uint64_t size,
		int send_fd_only)
{
	ssize_t len;

	if (!send_fd_only) {
		/* Send mmap size */
		len = ustcomm_send_unix_sock(sock, &size, sizeof(size));
		if (len != sizeof(size)) {
			if (len < 0)
				return len;
			else
				return -EIO;
		}

		/* Send channel type */
		len = ustcomm_send_unix_sock(sock, &type, sizeof(type));
		if (len != sizeof(type)) {
			if (len < 0)
				return len;
			else
				return -EIO;
		}
	}

	/* Send channel data */
	len = ustcomm_send_unix_sock(sock, data, size);
	if (len != size) {
		if (len < 0)
			return len;
		else
			return -EIO;
	}

	return 0;
}

static
int ustctl_send_stream(int sock,
		uint32_t stream_nr,
		uint64_t memory_map_size,
		int shm_fd, int wakeup_fd,
		int send_fd_only)
{
	ssize_t len;
	int fds[2];

	if (!send_fd_only) {
		if (shm_fd < 0) {
			/* finish iteration */
			uint64_t v = -1;

			len = ustcomm_send_unix_sock(sock, &v, sizeof(v));
			if (len != sizeof(v)) {
				if (len < 0)
					return len;
				else
					return -EIO;
			}
			return 0;
		}

		/* Send mmap size */
		len = ustcomm_send_unix_sock(sock, &memory_map_size,
			sizeof(memory_map_size));
		if (len != sizeof(memory_map_size)) {
			if (len < 0)
				return len;
			else
				return -EIO;
		}

		/* Send stream nr */
		len = ustcomm_send_unix_sock(sock, &stream_nr,
			sizeof(stream_nr));
		if (len != sizeof(stream_nr)) {
			if (len < 0)
				return len;
			else
				return -EIO;
		}
	}

	/* Send shm fd and wakeup fd */
	fds[0] = shm_fd;
	fds[1] = wakeup_fd;
	len = ustcomm_send_fds_unix_sock(sock, fds, 2);
	if (len <= 0) {
		if (len < 0)
			return len;
		else
			return -EIO;
	}
	return 0;
}

int ustctl_recv_channel_from_consumer(int sock,
		struct lttng_ust_object_data **_channel_data)
{
	struct lttng_ust_object_data *channel_data;
	ssize_t len;
	int ret;

	channel_data = zmalloc(sizeof(*channel_data));
	if (!channel_data) {
		ret = -ENOMEM;
		goto error_alloc;
	}
	channel_data->type = LTTNG_UST_OBJECT_TYPE_CHANNEL;

	/* recv mmap size */
	len = ustcomm_recv_unix_sock(sock, &channel_data->size,
			sizeof(channel_data->size));
	if (len != sizeof(channel_data->size)) {
		if (len < 0)
			ret = len;
		else
			ret = -EINVAL;
		goto error;
	}

	/* recv channel type */
	len = ustcomm_recv_unix_sock(sock, &channel_data->u.channel.type,
			sizeof(channel_data->u.channel.type));
	if (len != sizeof(channel_data->u.channel.type)) {
		if (len < 0)
			ret = len;
		else
			ret = -EINVAL;
		goto error;
	}

	/* recv channel data */
	channel_data->u.channel.data = zmalloc(channel_data->size);
	if (!channel_data->u.channel.data) {
		ret = -ENOMEM;
		goto error;
	}
	len = ustcomm_recv_unix_sock(sock, channel_data->u.channel.data,
			channel_data->size);
	if (len != channel_data->size) {
		if (len < 0)
			ret = len;
		else
			ret = -EINVAL;
		goto error_recv_data;
	}

	*_channel_data = channel_data;
	return 0;

error_recv_data:
	free(channel_data->u.channel.data);
error:
	free(channel_data);
error_alloc:
	return ret;
}

int ustctl_recv_stream_from_consumer(int sock,
		struct lttng_ust_object_data **_stream_data)
{
	struct lttng_ust_object_data *stream_data;
	ssize_t len;
	int ret;
	int fds[2];

	stream_data = zmalloc(sizeof(*stream_data));
	if (!stream_data) {
		ret = -ENOMEM;
		goto error_alloc;
	}

	stream_data->type = LTTNG_UST_OBJECT_TYPE_STREAM;
	stream_data->handle = -1;

	/* recv mmap size */
	len = ustcomm_recv_unix_sock(sock, &stream_data->size,
			sizeof(stream_data->size));
	if (len != sizeof(stream_data->size)) {
		if (len < 0)
			ret = len;
		else
			ret = -EINVAL;
		goto error;
	}
	if (stream_data->size == -1) {
		ret = -LTTNG_UST_ERR_NOENT;
		goto error;
	}

	/* recv stream nr */
	len = ustcomm_recv_unix_sock(sock, &stream_data->u.stream.stream_nr,
			sizeof(stream_data->u.stream.stream_nr));
	if (len != sizeof(stream_data->u.stream.stream_nr)) {
		if (len < 0)
			ret = len;
		else
			ret = -EINVAL;
		goto error;
	}

	/* recv shm fd and wakeup fd */
	len = ustcomm_recv_fds_unix_sock(sock, fds, 2);
	if (len <= 0) {
		if (len < 0) {
			ret = len;
			goto error;
		} else {
			ret = -EIO;
			goto error;
		}
	}
	stream_data->u.stream.shm_fd = fds[0];
	stream_data->u.stream.wakeup_fd = fds[1];
	*_stream_data = stream_data;
	return 0;

error:
	free(stream_data);
error_alloc:
	return ret;
}

int ustctl_send_channel_to_ust(int sock, int session_handle,
		struct lttng_ust_object_data *channel_data)
{
	struct ustcomm_ust_msg lum;
	struct ustcomm_ust_reply lur;
	int ret;

	if (!channel_data)
		return -EINVAL;

	memset(&lum, 0, sizeof(lum));
	lum.handle = session_handle;
	lum.cmd = LTTNG_UST_CHANNEL;
	lum.u.channel.len = channel_data->size;
	lum.u.channel.type = channel_data->u.channel.type;
	ret = ustcomm_send_app_msg(sock, &lum);
	if (ret)
		return ret;

	ret = ustctl_send_channel(sock,
			channel_data->u.channel.type,
			channel_data->u.channel.data,
			channel_data->size,
			1);
	if (ret)
		return ret;
	ret = ustcomm_recv_app_reply(sock, &lur, lum.handle, lum.cmd);
	if (!ret) {
		if (lur.ret_val >= 0) {
			channel_data->handle = lur.ret_val;
		}
	}
	return ret;
}

int ustctl_send_stream_to_ust(int sock,
		struct lttng_ust_object_data *channel_data,
		struct lttng_ust_object_data *stream_data)
{
	struct ustcomm_ust_msg lum;
	struct ustcomm_ust_reply lur;
	int ret;

	memset(&lum, 0, sizeof(lum));
	lum.handle = channel_data->handle;
	lum.cmd = LTTNG_UST_STREAM;
	lum.u.stream.len = stream_data->size;
	lum.u.stream.stream_nr = stream_data->u.stream.stream_nr;
	ret = ustcomm_send_app_msg(sock, &lum);
	if (ret)
		return ret;

	assert(stream_data);
	assert(stream_data->type == LTTNG_UST_OBJECT_TYPE_STREAM);

	ret = ustctl_send_stream(sock,
			stream_data->u.stream.stream_nr,
			stream_data->size,
			stream_data->u.stream.shm_fd,
			stream_data->u.stream.wakeup_fd, 1);
	if (ret)
		return ret;
	return ustcomm_recv_app_reply(sock, &lur, lum.handle, lum.cmd);
}


/* Buffer operations */

struct ustctl_consumer_channel *
	ustctl_create_channel(struct ustctl_consumer_channel_attr *attr)
{
	struct ustctl_consumer_channel *chan;
	const char *transport_name;
	struct lttng_transport *transport;

	switch (attr->type) {
	case LTTNG_UST_CHAN_PER_CPU:
		if (attr->output == LTTNG_UST_MMAP) {
			transport_name = attr->overwrite ?
				"relay-overwrite-mmap" : "relay-discard-mmap";
		} else {
			return NULL;
		}
		break;
	case LTTNG_UST_CHAN_METADATA:
		if (attr->output == LTTNG_UST_MMAP)
			transport_name = "relay-metadata-mmap";
		else
			return NULL;
		break;
	default:
		transport_name = "<unknown>";
		return NULL;
	}

	transport = lttng_transport_find(transport_name);
	if (!transport) {
		DBG("LTTng transport %s not found\n",
		       transport_name);
		return NULL;
	}

	chan = zmalloc(sizeof(*chan));
	if (!chan)
		return NULL;

	chan->chan = transport->ops.channel_create(transport_name, NULL,
                        attr->subbuf_size, attr->num_subbuf,
			attr->switch_timer_interval,
                        attr->read_timer_interval,
			attr->uuid);
	if (!chan->chan) {
		goto chan_error;
	}
	chan->chan->ops = &transport->ops;
	memcpy(&chan->attr, attr, sizeof(chan->attr));
	return chan;

chan_error:
	free(chan);
	return NULL;
}

void ustctl_destroy_channel(struct ustctl_consumer_channel *chan)
{
	chan->chan->ops->channel_destroy(chan->chan);
	free(chan);
}

int ustctl_send_channel_to_sessiond(int sock,
		struct ustctl_consumer_channel *channel)
{
	struct shm_object_table *table;

	table = channel->chan->handle->table;
	if (table->size <= 0)
		return -EINVAL;
	return ustctl_send_channel(sock,
			channel->attr.type,
			table->objects[0].memory_map,
			table->objects[0].memory_map_size,
			0);
}

int ustctl_send_stream_to_sessiond(int sock,
		struct ustctl_consumer_stream *stream)
{
	if (!stream)
		return ustctl_send_stream(sock, -1U, -1U, -1, -1, 0);

	return ustctl_send_stream(sock,
			stream->cpu,
			stream->memory_map_size,
			stream->shm_fd, stream->wakeup_fd,
			0);
}

int ustctl_stream_close_wait_fd(struct ustctl_consumer_stream *stream)
{
	struct channel *chan;

	chan = stream->chan->chan->chan;
	return ring_buffer_close_wait_fd(&chan->backend.config,
			chan, stream->handle, stream->cpu);
}

int ustctl_stream_close_wakeup_fd(struct ustctl_consumer_stream *stream)
{
	struct channel *chan;

	chan = stream->chan->chan->chan;
	return ring_buffer_close_wakeup_fd(&chan->backend.config,
			chan, stream->handle, stream->cpu);
}

struct ustctl_consumer_stream *
	ustctl_create_stream(struct ustctl_consumer_channel *channel,
			int cpu)
{
	struct ustctl_consumer_stream *stream;
	struct lttng_ust_shm_handle *handle;
	struct channel *chan;
	int shm_fd, wait_fd, wakeup_fd;
	uint64_t memory_map_size;
	struct lttng_ust_lib_ring_buffer *buf;
	int ret;

	if (!channel)
		return NULL;
	handle = channel->chan->handle;
	if (!handle)
		return NULL;

	chan = channel->chan->chan;
	buf = channel_get_ring_buffer(&chan->backend.config,
		chan, cpu, handle, &shm_fd, &wait_fd,
		&wakeup_fd, &memory_map_size);
	if (!buf)
		return NULL;
	ret = lib_ring_buffer_open_read(buf, handle);
	if (ret)
		return NULL;

	stream = zmalloc(sizeof(*stream));
	if (!stream)
		goto alloc_error;
	stream->handle = handle;
	stream->buf = buf;
	stream->chan = channel;
	stream->shm_fd = shm_fd;
	stream->wait_fd = wait_fd;
	stream->wakeup_fd = wakeup_fd;
	stream->memory_map_size = memory_map_size;
	stream->cpu = cpu;
	return stream;

alloc_error:
	return NULL;
}

void ustctl_destroy_stream(struct ustctl_consumer_stream *stream)
{
	struct lttng_ust_lib_ring_buffer *buf;
	struct ustctl_consumer_channel *consumer_chan;

	assert(stream);
	buf = stream->buf;
	consumer_chan = stream->chan;
	lib_ring_buffer_release_read(buf, consumer_chan->chan->handle);
	free(stream);
}

int ustctl_get_wait_fd(struct ustctl_consumer_stream *stream)
{
	struct lttng_ust_lib_ring_buffer *buf;
	struct ustctl_consumer_channel *consumer_chan;

	if (!stream)
		return -EINVAL;
	buf = stream->buf;
	consumer_chan = stream->chan;
	return shm_get_wait_fd(consumer_chan->chan->handle, &buf->self._ref);
}

int ustctl_get_wakeup_fd(struct ustctl_consumer_stream *stream)
{
	struct lttng_ust_lib_ring_buffer *buf;
	struct ustctl_consumer_channel *consumer_chan;

	if (!stream)
		return -EINVAL;
	buf = stream->buf;
	consumer_chan = stream->chan;
	return shm_get_wakeup_fd(consumer_chan->chan->handle, &buf->self._ref);
}

/* For mmap mode, readable without "get" operation */

void *ustctl_get_mmap_base(struct ustctl_consumer_stream *stream)
{
	struct lttng_ust_lib_ring_buffer *buf;
	struct ustctl_consumer_channel *consumer_chan;

	if (!stream)
		return NULL;
	buf = stream->buf;
	consumer_chan = stream->chan;
	return shmp(consumer_chan->chan->handle, buf->backend.memory_map);
}

/* returns the length to mmap. */
int ustctl_get_mmap_len(struct ustctl_consumer_stream *stream,
		unsigned long *len)
{
	struct ustctl_consumer_channel *consumer_chan;
	unsigned long mmap_buf_len;
	struct channel *chan;

	if (!stream)
		return -EINVAL;
	consumer_chan = stream->chan;
	chan = consumer_chan->chan->chan;
	if (chan->backend.config.output != RING_BUFFER_MMAP)
		return -EINVAL;
	mmap_buf_len = chan->backend.buf_size;
	if (chan->backend.extra_reader_sb)
		mmap_buf_len += chan->backend.subbuf_size;
	if (mmap_buf_len > INT_MAX)
		return -EFBIG;
	*len = mmap_buf_len;
	return 0;
}

/* returns the maximum size for sub-buffers. */
int ustctl_get_max_subbuf_size(struct ustctl_consumer_stream *stream,
		unsigned long *len)
{
	struct ustctl_consumer_channel *consumer_chan;
	struct channel *chan;

	if (!stream)
		return -EINVAL;
	consumer_chan = stream->chan;
	chan = consumer_chan->chan->chan;
	*len = chan->backend.subbuf_size;
	return 0;
}

/*
 * For mmap mode, operate on the current packet (between get/put or
 * get_next/put_next).
 */

/* returns the offset of the subbuffer belonging to the mmap reader. */
int ustctl_get_mmap_read_offset(struct ustctl_consumer_stream *stream,
		unsigned long *off)
{
	struct channel *chan;
	unsigned long sb_bindex;
	struct lttng_ust_lib_ring_buffer *buf;
	struct ustctl_consumer_channel *consumer_chan;

	if (!stream)
		return -EINVAL;
	buf = stream->buf;
	consumer_chan = stream->chan;
	chan = consumer_chan->chan->chan;
	if (chan->backend.config.output != RING_BUFFER_MMAP)
		return -EINVAL;
	sb_bindex = subbuffer_id_get_index(&chan->backend.config,
					   buf->backend.buf_rsb.id);
	*off = shmp(consumer_chan->chan->handle,
		shmp_index(consumer_chan->chan->handle, buf->backend.array, sb_bindex)->shmp)->mmap_offset;
	return 0;
}

/* returns the size of the current sub-buffer, without padding (for mmap). */
int ustctl_get_subbuf_size(struct ustctl_consumer_stream *stream,
		unsigned long *len)
{
	struct ustctl_consumer_channel *consumer_chan;
	struct channel *chan;
	struct lttng_ust_lib_ring_buffer *buf;

	if (!stream)
		return -EINVAL;

	buf = stream->buf;
	consumer_chan = stream->chan;
	chan = consumer_chan->chan->chan;
	*len = lib_ring_buffer_get_read_data_size(&chan->backend.config, buf,
		consumer_chan->chan->handle);
	return 0;
}

/* returns the size of the current sub-buffer, without padding (for mmap). */
int ustctl_get_padded_subbuf_size(struct ustctl_consumer_stream *stream,
		unsigned long *len)
{
	struct ustctl_consumer_channel *consumer_chan;
	struct channel *chan;
	struct lttng_ust_lib_ring_buffer *buf;

	if (!stream)
		return -EINVAL;
	buf = stream->buf;
	consumer_chan = stream->chan;
	chan = consumer_chan->chan->chan;
	*len = lib_ring_buffer_get_read_data_size(&chan->backend.config, buf,
		consumer_chan->chan->handle);
	*len = PAGE_ALIGN(*len);
	return 0;
}

/* Get exclusive read access to the next sub-buffer that can be read. */
int ustctl_get_next_subbuf(struct ustctl_consumer_stream *stream)
{
	struct lttng_ust_lib_ring_buffer *buf;
	struct ustctl_consumer_channel *consumer_chan;

	if (!stream)
		return -EINVAL;
	buf = stream->buf;
	consumer_chan = stream->chan;
	return lib_ring_buffer_get_next_subbuf(buf,
			consumer_chan->chan->handle);
}


/* Release exclusive sub-buffer access, move consumer forward. */
int ustctl_put_next_subbuf(struct ustctl_consumer_stream *stream)
{
	struct lttng_ust_lib_ring_buffer *buf;
	struct ustctl_consumer_channel *consumer_chan;

	if (!stream)
		return -EINVAL;
	buf = stream->buf;
	consumer_chan = stream->chan;
	lib_ring_buffer_put_next_subbuf(buf, consumer_chan->chan->handle);
	return 0;
}

/* snapshot */

/* Get a snapshot of the current ring buffer producer and consumer positions */
int ustctl_snapshot(struct ustctl_consumer_stream *stream)
{
	struct lttng_ust_lib_ring_buffer *buf;
	struct ustctl_consumer_channel *consumer_chan;

	if (!stream)
		return -EINVAL;
	buf = stream->buf;
	consumer_chan = stream->chan;
	return lib_ring_buffer_snapshot(buf, &buf->cons_snapshot,
			&buf->prod_snapshot, consumer_chan->chan->handle);
}

/* Get the consumer position (iteration start) */
int ustctl_snapshot_get_consumed(struct ustctl_consumer_stream *stream,
		unsigned long *pos)
{
	struct lttng_ust_lib_ring_buffer *buf;

	if (!stream)
		return -EINVAL;
	buf = stream->buf;
	*pos = buf->cons_snapshot;
	return 0;
}

/* Get the producer position (iteration end) */
int ustctl_snapshot_get_produced(struct ustctl_consumer_stream *stream,
		unsigned long *pos)
{
	struct lttng_ust_lib_ring_buffer *buf;

	if (!stream)
		return -EINVAL;
	buf = stream->buf;
	*pos = buf->prod_snapshot;
	return 0;
}

/* Get exclusive read access to the specified sub-buffer position */
int ustctl_get_subbuf(struct ustctl_consumer_stream *stream,
		unsigned long *pos)
{
	struct lttng_ust_lib_ring_buffer *buf;
	struct ustctl_consumer_channel *consumer_chan;

	if (!stream)
		return -EINVAL;
	buf = stream->buf;
	consumer_chan = stream->chan;
	return lib_ring_buffer_get_subbuf(buf, *pos,
			consumer_chan->chan->handle);
}

/* Release exclusive sub-buffer access */
int ustctl_put_subbuf(struct ustctl_consumer_stream *stream)
{
	struct lttng_ust_lib_ring_buffer *buf;
	struct ustctl_consumer_channel *consumer_chan;

	if (!stream)
		return -EINVAL;
	buf = stream->buf;
	consumer_chan = stream->chan;
	lib_ring_buffer_put_subbuf(buf, consumer_chan->chan->handle);
	return 0;
}

void ustctl_flush_buffer(struct ustctl_consumer_stream *stream,
		int producer_active)
{
	struct lttng_ust_lib_ring_buffer *buf;
	struct ustctl_consumer_channel *consumer_chan;

	assert(stream);
	buf = stream->buf;
	consumer_chan = stream->chan;
	lib_ring_buffer_switch_slow(buf,
		producer_active ? SWITCH_ACTIVE : SWITCH_FLUSH,
		consumer_chan->chan->handle);
}

static __attribute__((constructor))
void ustctl_init(void)
{
	init_usterr();
	lttng_ring_buffer_metadata_client_init();
	lttng_ring_buffer_client_overwrite_init();
	lttng_ring_buffer_client_discard_init();
}

static __attribute__((destructor))
void ustctl_exit(void)
{
	lttng_ring_buffer_client_discard_exit();
	lttng_ring_buffer_client_overwrite_exit();
	lttng_ring_buffer_metadata_client_exit();
}
