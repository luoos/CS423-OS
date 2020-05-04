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
	int attr_max_len = 64;
	char attr_val[attr_max_len];
	struct dentry *dentry;
	int len;
	int sid = MP4_NO_ACCESS;

	if (!inode || !inode->i_op->getxattr) {
		return sid;
	}

	dentry = d_find_alias(inode);

	if (!dentry) {
		return sid;
	}

	len = inode->i_op->getxattr(dentry, XATTR_MP4_SUFFIX, attr_val, attr_max_len);

	if (len > 0) {
		sid = __cred_ctx_to_sid(attr_val);
		if (sid == MP4_NO_ACCESS) {
			pr_info("[Warning] return MP4_NO_ACCESS with %s", attr_val);
		}
	}
	dput(dentry);
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
		pr_info("mp4_cred_prepare no new");
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
		return -EOPNOTSUPP;
	}

	if (cur_blob->mp4_flags == MP4_TARGET_SID) {
		if (name && value && len) {
			*name  = XATTR_MP4_SUFFIX;
			*value = XATTR_VALUE_RW;
			*len   = strlen(*value);
			return 0;
		} else if (printk_ratelimit()) {
			pr_alert("mp4_inode_init_security invalid params, name: %p, value: %p, len: %p",
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
	/*
	 * Add your code here
	 * ...
	 */
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
	/*
	 * Add your code here
	 * ...
	 */
	return 0;
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

	pr_info("mp4 LSM initializing..");

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
