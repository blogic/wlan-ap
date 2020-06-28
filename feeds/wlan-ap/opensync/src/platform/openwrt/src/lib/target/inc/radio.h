#ifndef _RADIO_H__
#define _RADIO_H__

extern const struct target_radio_ops *radio_ops;
extern int reload_config;
extern struct blob_buf b;
extern struct uci_context *uci;

#endif
