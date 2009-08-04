#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
 .name = KBUILD_MODNAME,
 .init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
 .exit = cleanup_module,
#endif
 .arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0x425191a5, "struct_module" },
	{ 0xfe1bb408, "netif_rx" },
	{ 0x14c3b0b3, "skb_push" },
	{ 0x2e69a554, "skb_put" },
	{ 0xa13798f8, "printk_ratelimit" },
	{ 0xe26243bf, "dev_alloc_skb" },
	{ 0x7d11c268, "jiffies" },
	{ 0x6f7507d9, "kfree_skb" },
	{ 0x12da5bb2, "__kmalloc" },
	{ 0x9d669763, "memcpy" },
	{ 0xfa2a45e, "__memzero" },
	{ 0x5f754e5a, "memset" },
	{ 0x7ea3755e, "register_netdev" },
	{ 0x71b98b1e, "alloc_netdev_mq" },
	{ 0x7739d11, "tty_register_ldisc" },
	{ 0xa3283a05, "free_netdev" },
	{ 0xc9eb2b22, "unregister_netdev" },
	{ 0x8728140, "malloc_sizes" },
	{ 0xdc74cc24, "kmem_cache_alloc" },
	{ 0x37a0cba, "kfree" },
	{ 0x1ca32a01, "module_put" },
	{ 0xea147363, "printk" },
	{ 0xa439ba97, "__netif_schedule" },
	{ 0x1902adf, "netpoll_trap" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "3046D10E0AD4D8326AB7899");
