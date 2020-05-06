#define pr_fmt(fmt) "cs423_mp4: " fmt

#include <linux/lsm_hooks.h>
#include <linux/security.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/cred.h>
#include <linux/dcache.h>
#include <linux/binfmts.h>
#include "mp4_given.h"

/**
 * get_inode_sid - Get the inode mp4 security label id
 *
 * @inode: the input inode
 *
 * @return the inode's security id if found.
 *
 */
static int get_inode_sid(struct inode *inode)
{
	int buflen = 256;
	char *buf;
	struct dentry *dentry;
	int ret;
	int sid = MP4_NO_ACCESS;

	if (!inode->i_op->getxattr) {
		return sid;
	}

	dentry = d_find_alias(inode);

	if (!dentry) {
		return sid;
	}

	buf = kmalloc(buflen, GFP_KERNEL);
	memset(buf, 0, buflen); // reall needed? set the buf[len] to '\0' should be enough

	ret = inode->i_op->getxattr(dentry, XATTR_NAME_MP4, buf, buflen);

	if (ret == -ERANGE) { // don't understand how ERANGE works
		kfree(buf);
		ret = inode->i_op->getxattr(dentry, XATTR_NAME_MP4, NULL, 0);
		if (ret < 0) {
			dput(dentry);
			return sid;
		}
		buf = kmalloc(ret + 1, GFP_KERNEL);
		ret = inode->i_op->getxattr(dentry, XATTR_NAME_MP4, buf, buflen);
		buf[ret] = 0;
	}
	dput(dentry);
	if (ret > 0) {
		sid = __cred_ctx_to_sid(buf);
	}
	kfree(buf);
	return sid;
}

/**
 * mp4_cred_alloc_blank - Allocate a blank mp4 security label
 *
 * @cred: the new credentials
 * @gfp: the atomicity of the memory allocation
 *
 */
static int mp4_cred_alloc_blank(struct cred *cred, gfp_t gfp)
{
	struct mp4_security *security_blob;
	if (!cred) {
		return 0;
	}
	security_blob = kmalloc(sizeof(struct mp4_security), gfp);
	if (!security_blob) {
		return -ENOMEM;
	}

	security_blob->mp4_flags = MP4_NO_ACCESS;
	cred->security = security_blob;
	return 0;
}

/**
 * mp4_bprm_set_creds - Set the credentials for a new task
 *
 * @bprm: The linux binary preparation structure
 *
 * returns 0 on success.
 */
static int mp4_bprm_set_creds(struct linux_binprm *bprm)
{
	struct inode *inode;
	struct mp4_security *blob;
	int sid;

	if (!bprm || !bprm->file || !bprm->file->f_inode || !bprm->cred) {
		return -EINVAL;
	}

	inode = bprm->file->f_inode;

	sid = get_inode_sid(inode);

	if (sid == MP4_TARGET_SID) {
		if (!bprm->cred->security) {
			mp4_cred_alloc_blank(bprm->cred, GFP_KERNEL);
		}

		blob = bprm->cred->security;
		blob->mp4_flags = sid;
	}

	return 0;
}

/**
 * mp4_cred_free - Free a created security label
 *
 * @cred: the credentials struct
 *
 */
static void mp4_cred_free(struct cred *cred)
{
	if (!cred || !cred->security) {
		return;
	}

	kfree(cred->security);
	cred->security = NULL;
}

/**
 * mp4_cred_prepare - Prepare new credentials for modification
 *
 * @new: the new credentials
 * @old: the old credentials
 * @gfp: the atomicity of the memory allocation
 *
 */
static int mp4_cred_prepare(struct cred *new, const struct cred *old,
			    gfp_t gfp)
{
	struct mp4_security *old_blob, *new_blob;
	if (!new) {
		pr_info("mp4_cred_prepare no new\n");
		return 0;
	}

	new_blob = kmalloc(sizeof(struct mp4_security), gfp);
	if (!new_blob) {
		return -ENOMEM;
	}

	if (!old || !old->security) {
		new_blob->mp4_flags = MP4_NO_ACCESS;
	} else {
		old_blob = old->security;
		new_blob->mp4_flags = old_blob->mp4_flags;
	}
	new->security = new_blob;
	return 0;
}

/**
 * mp4_inode_init_security - Set the security attribute of a newly created inode
 *
 * @inode: the newly created inode
 * @dir: the containing directory
 * @qstr: unused
 * @name: where to put the attribute name
 * @value: where to put the attribute value
 * @len: where to put the length of the attribute
 *
 * returns 0 if all goes well, -ENOMEM if no memory, -EOPNOTSUPP to skip
 *
 */
static int mp4_inode_init_security(struct inode *inode, struct inode *dir,
				   const struct qstr *qstr,
				   const char **name, void **value, size_t *len)
{
	struct mp4_security *cur_blob;

	cur_blob = current_security();
	if (!cur_blob) {
		return 0;
	}

	if (cur_blob->mp4_flags == MP4_TARGET_SID) {
		if (name && value && len) {
			*name = kstrdup(XATTR_MP4_SUFFIX, GFP_KERNEL);
			if (S_ISDIR(inode->i_mode)) {
				*value = kstrdup("dir-write", GFP_KERNEL);
				*len = 10;
			} else {
				*value = kstrdup("read-write", GFP_KERNEL);
				*len = 11;
			}
			return 0;
		} else if (printk_ratelimit()) {
			pr_alert("mp4_inode_init_security invalid params, name: %p, value: %p, len: %p\n",
					 name, value, len);
		}
	}
	return -EOPNOTSUPP;
}

/**
 * mp4_has_permission - Check if subject has permission to an object
 *
 * @ssid: the subject's security id
 * @osid: the object's security id
 * @mask: the operation mask
 *
 * returns 0 is access granter, -EACCES otherwise
 *
 */
static int mp4_has_permission(int ssid, int osid, int mask)
{
	int is_target = (ssid == MP4_TARGET_SID);
	switch(osid) {
		case MP4_NO_ACCESS:
			if (is_target) {
				return -EACCES;
			}
			break;
		case MP4_READ_OBJ:
			if (mask & (MAY_WRITE|MAY_APPEND|MAY_EXEC)) {
				return -EACCES;
			}
			break;
		case MP4_READ_WRITE:
			if (is_target && (mask & MAY_EXEC)) {
				return -EACCES;
			} else if (!is_target && (mask & (MAY_WRITE|MAY_APPEND|MAY_EXEC))) {
				return -EACCES;
			}
			break;
		case MP4_WRITE_OBJ:
			if (is_target && (mask & (MAY_READ|MAY_EXEC))) {
				return -EACCES;
			} else if (!is_target && (mask & (MAY_WRITE|MAY_APPEND|MAY_EXEC))) {
				return -EACCES;
			}
			break;
		case MP4_EXEC_OBJ:
			if (mask & (MAY_WRITE|MAY_APPEND)) {
				return -EACCES;
			}
			break;
		case MP4_READ_DIR:
			if (is_target && (mask & (MAY_WRITE|MAY_APPEND))) {
				return -EACCES;
			}
			break;
	}
	return 0;
}

/**
 * mp4_inode_permission - Check permission for an inode being opened
 *
 * @inode: the inode in question
 * @mask: the access requested
 *
 * This is the important access check hook
 *
 * returns 0 if access is granted, -EACCES otherwise
 *
 */
static int mp4_inode_permission(struct inode *inode, int mask)
{
	char *path;
	char *buf;
	int buflen = 256;
	int ssid = 0, osid = 0;
	int access_code = 0;
	struct dentry *dentry;
	struct mp4_security *blob;

	mask &= (MAY_READ|MAY_WRITE|MAY_EXEC|MAY_APPEND);

	if (mask == 0) {
		return 0;
	}

	if (!inode) {
		if (printk_ratelimit()) {
			pr_alert("inode_permission: inode is NULL\n");
		}
		return 0;
	}

	dentry = d_find_alias(inode);
	if (!dentry) {
		if (printk_ratelimit()) {
			pr_alert("inode_permission: dentry is NULL\n");
		}
		return 0;
	}

	buf = kmalloc(buflen, GFP_KERNEL);
	path = dentry_path_raw(dentry, buf, buflen);
	if (path && mp4_should_skip_path(path)) {
		dput(dentry);
		kfree(buf);
		return 0;
	}
	dput(dentry);

	blob = current_security();
	if (blob) {
		ssid = blob->mp4_flags;
	}
	osid = get_inode_sid(inode);

	if (ssid != MP4_TARGET_SID && S_ISDIR(inode->i_mode)) {
		kfree(buf);
		return 0;
	}

	access_code = mp4_has_permission(ssid, osid, mask);
	if (access_code) {
		// deny
		pr_alert("access denied, ssid: %d, osid: %d, mask: %d, path: %s\n", ssid, osid, mask, path);
	}
	kfree(buf);
	return access_code;
}


/*
 * This is the list of hooks that we will using for our security module.
 */
static struct security_hook_list mp4_hooks[] = {
	/*
	 * inode function to assign a label and to check permission
	 */
	LSM_HOOK_INIT(inode_init_security, mp4_inode_init_security),
	LSM_HOOK_INIT(inode_permission, mp4_inode_permission),

	/*
	 * setting the credentials subjective security label when laucnhing a
	 * binary
	 */
	LSM_HOOK_INIT(bprm_set_creds, mp4_bprm_set_creds),

	/* credentials handling and preparation */
	LSM_HOOK_INIT(cred_alloc_blank, mp4_cred_alloc_blank),
	LSM_HOOK_INIT(cred_free, mp4_cred_free),
	LSM_HOOK_INIT(cred_prepare, mp4_cred_prepare)
};

static __init int mp4_init(void)
{
	/*
	 * check if mp4 lsm is enabled with boot parameters
	 */
	if (!security_module_enable("mp4"))
		return 0;

	pr_info("mp4 LSM initializing..\n");

	/*
	 * Register the mp4 hooks with lsm
	 */
	security_add_hooks(mp4_hooks, ARRAY_SIZE(mp4_hooks));

	return 0;
}

/*
 * early registration with the kernel
 */
security_initcall(mp4_init);
