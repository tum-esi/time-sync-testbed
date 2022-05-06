#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0xeeab4c1e, "module_layout" },
	{ 0x695f5e06, "param_ops_charp" },
	{ 0x93c68c79, "param_ops_int" },
	{ 0x3fd96790, "pci_unregister_driver" },
	{ 0x5dcd13e, "__pci_register_driver" },
	{ 0xe2d5255a, "strcmp" },
	{ 0xc5850110, "printk" },
	{ 0x2f51c2e0, "_dev_warn" },
	{ 0xde80cd09, "ioremap" },
	{ 0x84c43e1a, "dma_free_attrs" },
	{ 0xf4470c00, "dma_alloc_attrs" },
	{ 0x9b8b2c09, "__uio_register_device" },
	{ 0x10928e02, "sysfs_create_group" },
	{ 0x7f5d4a89, "dma_set_coherent_mask" },
	{ 0x7b1bfe78, "dma_set_mask" },
	{ 0x9b1c2d05, "pci_enable_device" },
	{ 0xd60e87c0, "kmem_cache_alloc_trace" },
	{ 0x6aa9e858, "kmalloc_caches" },
	{ 0x2ef1c508, "pci_msi_unmask_irq" },
	{ 0xec4eb95f, "pci_intx" },
	{ 0xba7b230, "pci_msi_mask_irq" },
	{ 0xf6e4d0c8, "pci_cfg_access_unlock" },
	{ 0x85fbeafd, "pci_cfg_access_lock" },
	{ 0x36c6c81, "irq_get_irq_data" },
	{ 0x1e85fa69, "_dev_notice" },
	{ 0x529e559b, "_dev_err" },
	{ 0xac4b3ae, "__dynamic_dev_dbg" },
	{ 0xfdb99fa8, "pci_irq_vector" },
	{ 0xa1d5f9b3, "pci_alloc_irq_vectors_affinity" },
	{ 0xf9dd1bc6, "_dev_info" },
	{ 0x92d5838e, "request_threaded_irq" },
	{ 0xc56b10bc, "pci_set_master" },
	{ 0x3babaad, "pci_check_and_mask_intx" },
	{ 0x9f62da09, "uio_event_notify" },
	{ 0x37a0cba, "kfree" },
	{ 0xa0791bac, "pci_disable_device" },
	{ 0xedc03953, "iounmap" },
	{ 0x4dd5f20b, "uio_unregister_device" },
	{ 0x47340d9, "sysfs_remove_group" },
	{ 0xc41040b0, "pci_free_irq_vectors" },
	{ 0xc1514a3b, "free_irq" },
	{ 0x228343da, "pci_clear_master" },
	{ 0x656e4a6e, "snprintf" },
	{ 0x2ea2c95c, "__x86_indirect_thunk_rax" },
	{ 0xc959d152, "__stack_chk_fail" },
	{ 0x5191a0b7, "pci_enable_sriov" },
	{ 0xaeddbc52, "pci_disable_sriov" },
	{ 0xff198bb8, "pci_num_vf" },
	{ 0x5c3c7387, "kstrtoull" },
	{ 0xbdfb6dbb, "__fentry__" },
};

MODULE_INFO(depends, "uio");


MODULE_INFO(srcversion, "F325335EB4CAE146326BB1F");
