/************************************************************************************
 ** File: - source\android\kernel\msm-5.4\drivers\input\oplus_secure_drivers\oplus_secure_common\oplus_secure_common.c
 ** OPLUS_FEATURE_SECURITY_COMMON
 ** Copyright (C), 2020-2025, OPLUS Mobile Comm Corp., Ltd
 **
 ** Description:
 **      secure_common compatibility configuration
 **
 ** Version: 1.0
 ** Date created: 18:03:11,02/11/2017
 **
 ** --------------------------- Revision History: --------------------------------
 **  <author>         <data>         <desc>
 **  Bin.Li         2017/11/17     create the file
 **  Bin.Li         2017/11/18     add for mt6771
 **  Ziqing.guo     2018/03/12     fix the problem for coverity CID 16731
 **  Hongdao.yu     2018/05/01     remove fp engineering mode
 **  Ping.Liu       2018/06/22     compatible with SDM670/SDM710.
 **  Long.Liu       2018/11/23     compatible with P80
 **  oujinrong      2018/12/29     compatible with SDM855
 **  oujinrong      2018/01/08     fix stage 1 machine with apdp boot failed
 **  oujinrong      2018/03/16     fix stage 2 machine boot failed
 **  Dongnan.Wu     2019/06/12     add 7150 platform support
 **  Ping.Liu       2019/10/16     add 7250 platform support.
 **  Dongnan.Wu     2020/08/07     modify proc inode name
 **  Dongnan.Wu     2020/08/25     modify device & drive inode name
 **  Bin.Li         2020/09/07     modify for secure common KO
 **  Junwei.You     2021/12/02     remove useless kernel log
 ************************************************************************************/

#include <linux/module.h>
#include <linux/proc_fs.h>

#include <linux/uaccess.h>
#include <asm/uaccess.h>

#include <linux/soc/qcom/smem.h>

#include <linux/slab.h>
#include <linux/seq_file.h>
#include <linux/fs.h>
#include <linux/of_gpio.h>

#include <linux/delay.h>
#include <linux/string.h>
#include <linux/err.h>
#include "../include/oplus_secure_common.h"

#define OEM_FUSE_OFF        "0"
#define OEM_FUSE_ON         "1"
#define UNKNOW_FUSE_VALUE   "unkown fuse"
#define FUSE_VALUE_LEN      15

#define SEC_REG_NODE       "oplus,sec_reg_num"
#define SEC_ENABLE_ANTIROLLBACK_REG       "oplus,sec_en_anti_reg"
#define SEC_OVERRIDE_1_REG       "oplus,sec_override1_reg"
#define OVERRIDE_1_ENABLED_VALUE       "oplus,override1_en_value"

static struct proc_dir_entry *oplus_secure_common_dir = NULL;
static char* oplus_secure_common_dir_name = "oplus_secure_common";
static struct secure_data *secure_data_ptr = NULL;
static char g_fuse_value[FUSE_VALUE_LEN] = UNKNOW_FUSE_VALUE ;
static uint32_t oem_sec_reg_num = 0;
static uint32_t oem_sec_en_anti_reg = 0;
static uint32_t oem_sec_override1_reg = 0;
static uint32_t oem_override1_en_value = 0;

static int secure_common_parse_parent_dts(struct secure_data *secure_data)
{
    int ret = SECURE_OK;
    struct device *dev = NULL;
    struct device_node *np = NULL;

    if (!secure_data || !secure_data->dev) {
        ret = -SECURE_ERROR_GENERAL;
        goto exit;
    }
    dev = secure_data->dev;
    np = dev->of_node;

    ret = of_property_read_u32(np, SEC_REG_NODE, &(secure_data->sec_reg_num));
    if (ret) {
        dev_err(secure_data->dev, "the param %s is not found !\n", SEC_REG_NODE);
        ret = -SECURE_ERROR_GENERAL;
        goto exit;
    }

    ret = of_property_read_u32(np, SEC_ENABLE_ANTIROLLBACK_REG, &(secure_data->sec_en_anti_reg));
    if (ret) {
        dev_err(secure_data->dev, "the param %s is not found !\n", SEC_ENABLE_ANTIROLLBACK_REG);
        ret = -SECURE_ERROR_GENERAL;
        goto exit;
    }

    ret = of_property_read_u32(np, SEC_OVERRIDE_1_REG, &(secure_data->sec_override1_reg));
    if (ret) {
        dev_err(secure_data->dev, "the param %s is not found !\n", SEC_OVERRIDE_1_REG);
        ret = -SECURE_ERROR_GENERAL;
        goto exit;
    }

    ret = of_property_read_u32(np, OVERRIDE_1_ENABLED_VALUE, &(secure_data->override1_en_value));
    if (ret) {
        dev_err(secure_data->dev, "the param %s is not found !\n", OVERRIDE_1_ENABLED_VALUE);
        ret = -SECURE_ERROR_GENERAL;
        goto exit;
    }
    oem_sec_reg_num = secure_data->sec_reg_num;
    oem_sec_en_anti_reg = secure_data->sec_en_anti_reg;
    oem_sec_override1_reg = secure_data->sec_override1_reg;
    oem_override1_en_value = secure_data->override1_en_value;
    dev_info(secure_data->dev, "sec_reg_num: %d sec_en_anti_reg: %d sec_override1_reg: %d override1_en_value: %d\n", secure_data->sec_reg_num, secure_data->sec_en_anti_reg, secure_data->sec_override1_reg, secure_data->override1_en_value);

exit:
    return ret;
}

secure_type_t get_secureType(void)
{
        secure_type_t secureType = SECURE_BOOT_UNKNOWN;
        #if defined(MTK_PLATFORM)
        if (strstr(saved_command_line, "androidboot.sbootstate=on")) {
                secureType = SECURE_BOOT_ON;
        } else {
                secureType = SECURE_BOOT_OFF;
        }
        #else 
        void __iomem *oem_config_base;
        uint32_t secure_oem_config1 = 0;
        uint32_t secure_oem_config2 = 0;
        oem_config_base = ioremap(oem_sec_reg_num, 4);
        secure_oem_config1 = __raw_readl(oem_config_base);
        iounmap(oem_config_base);

        oem_config_base = ioremap(oem_sec_en_anti_reg, 4);
        secure_oem_config2 = __raw_readl(oem_config_base);
        iounmap(oem_config_base);
        if (secure_oem_config1 == 0) {
                secureType = SECURE_BOOT_OFF;
        } else if (secure_oem_config2 == 0) {
                secureType = SECURE_BOOT_ON_STAGE_1;
        } else {
                secureType = SECURE_BOOT_ON_STAGE_2;
        }
        #endif
        return secureType;
}

static ssize_t secureType_read_proc(struct file *file, char __user *buf,
                size_t count, loff_t *off)
{
        char page[256] = {0};
        int len = 0;
        secure_type_t secureType = get_secureType();

        len = sprintf(page, "%d", secureType);

        if (len > *off) {
                len -= *off;
        }
        else {
                len = 0;
        }
        if (copy_to_user(buf, page, (len < count ? len : count))) {
                return -EFAULT;
        }
        *off += len < count ? len : count;
        return (len < count ? len : count);
}

static ssize_t secureType_write_proc(struct file *filp, const char __user *buf,
                size_t count, loff_t *offp)
{
        size_t local_count;
        if (count <= 0) {
                return 0;
        }
        strcpy(g_fuse_value, UNKNOW_FUSE_VALUE);

        local_count = (FUSE_VALUE_LEN - 1) < count ? (FUSE_VALUE_LEN - 1) : count;
        if (copy_from_user(g_fuse_value , buf, local_count) != 0) {
                dev_err(secure_data_ptr->dev, "write oem fuse value fail\n");
                return -EFAULT;
        }
        g_fuse_value[local_count] = '\0';
        dev_info(secure_data_ptr->dev, "write oem fuse value = %s\n", g_fuse_value);
        return count;
}

static struct file_operations secureType_proc_fops = {
        .read = secureType_read_proc,
        .write = secureType_write_proc,
};

static ssize_t secureSNBound_read_proc(struct file *file, char __user *buf,
                size_t count, loff_t *off)
{
    char page[256] = {0};
    int len = 0;
    secure_device_sn_bound_state_t secureSNBound_state = SECURE_DEVICE_SN_BOUND_UNKNOWN;

    if (oem_sec_override1_reg == 0x7860C4) {
        void __iomem *oem_config_base;
        uint32_t secure_override1_config = 0;
        oem_config_base = ioremap(oem_sec_override1_reg, 4);
        secure_override1_config = __raw_readl(oem_config_base);
        iounmap(oem_config_base);
        dev_info(secure_data_ptr->dev,"secure_override1_config 0x%x\n", secure_override1_config);

        if (get_secureType() == SECURE_BOOT_ON_STAGE_2 && secure_override1_config != oem_override1_en_value) {
                secureSNBound_state = SECURE_DEVICE_SN_BOUND_OFF; /*secure stage2 devices not bind serial number*/
        } else {
                secureSNBound_state = SECURE_DEVICE_SN_BOUND_ON;  /*secure stage2 devices bind serial number*/
        }
    } else {
        if (get_secureType() == SECURE_BOOT_ON_STAGE_2) {
                secureSNBound_state = SECURE_DEVICE_SN_BOUND_OFF;
        }
    }

    len = sprintf(page, "%d", secureSNBound_state);
    if (len > *off) {
        len -= *off;
    }
    else {
        len = 0;
    }

    if (copy_to_user(buf, page, (len < count ? len : count))) {
        return -EFAULT;
    }
    *off += len < count ? len : count;
    return (len < count ? len : count);
}


static struct file_operations secureSNBound_proc_fops = {
        .read = secureSNBound_read_proc,
};

static int secure_register_proc_fs(struct secure_data *secure_data)
{
        struct proc_dir_entry *pentry;

        /*  make the dir /proc/oplus_secure_common  */
        oplus_secure_common_dir =  proc_mkdir(oplus_secure_common_dir_name, NULL);
        if(!oplus_secure_common_dir) {
                dev_err(secure_data->dev,"can't create oplus_secure_common_dir proc\n");
                return SECURE_ERROR_GENERAL;
        }

        /*  make the proc /proc/oplus_secure_common/secureType  */
        pentry = proc_create("secureType", 0664, oplus_secure_common_dir, &secureType_proc_fops);
        if(!pentry) {
                dev_err(secure_data->dev,"create secureType proc failed.\n");
                return SECURE_ERROR_GENERAL;
        }

        /*  make the proc /proc/oplus_secure_common/secureSNBound  */
        pentry = proc_create("secureSNBound", 0444, oplus_secure_common_dir, &secureSNBound_proc_fops);
        if(!pentry) {
                dev_err(secure_data->dev,"create secureSNBound proc failed.\n");
                return SECURE_ERROR_GENERAL;
        }

        return SECURE_OK;
}

static int oplus_secure_common_probe(struct platform_device *secure_dev)
{
        int ret = 0;
        struct device *dev = &secure_dev->dev;
        struct secure_data *secure_data = NULL;

        secure_data = devm_kzalloc(dev,sizeof(struct secure_data), GFP_KERNEL);
        if (secure_data == NULL) {
                dev_err(dev,"secure_data kzalloc failed\n");
                ret = -ENOMEM;
                goto exit;
        }

        secure_data->dev = dev;
        secure_data_ptr = secure_data;

        //add to get the parent dts oplus_secure_common
        ret = secure_common_parse_parent_dts(secure_data);
        if (ret) {
                goto exit;
        }

        ret = secure_register_proc_fs(secure_data);
        if (ret) {
                goto exit;
        }
        return SECURE_OK;

exit:

        if (oplus_secure_common_dir) {
                remove_proc_entry(oplus_secure_common_dir_name, NULL);
        }

        dev_err(dev,"secure_data probe failed ret = %d\n",ret);
        if (secure_data) {
                devm_kfree(dev, secure_data);
        }

        return ret;
}

static int oplus_secure_common_remove(struct platform_device *pdev)
{
        return SECURE_OK;
}

static struct of_device_id oplus_secure_common_match_table[] = {
        {   .compatible = "oplus,secure_common", },
        {}
};

static struct platform_driver oplus_secure_common_driver = {
        .probe = oplus_secure_common_probe,
        .remove = oplus_secure_common_remove,
        .driver = {
                .name = "oplus_secure_common",
                .owner = THIS_MODULE,
                .of_match_table = oplus_secure_common_match_table,
        },
};

static int __init oplus_secure_common_init(void)
{
        return platform_driver_register(&oplus_secure_common_driver);
}

static void __exit oplus_secure_common_exit(void)
{
        platform_driver_unregister(&oplus_secure_common_driver);
}

fs_initcall(oplus_secure_common_init);
module_exit(oplus_secure_common_exit)
MODULE_DESCRIPTION("oplus secure common driver");
MODULE_LICENSE("GPL");
