/* SPDX-License-Identifier: GPL-2.0-or-later */
/* 
 * KPM Module Template
 * 使用说明:
 * 1. 复制 template 目录并重命名为你的模块名
 * 2. 修改此文件，实现你的功能
 * 3. 运行 ./build.sh 自动构建所有模块
 */

#include <compiler.h>
#include <kpmodule.h>
#include <linux/printk.h>
#include <common.h>
#include <kputils.h>
#include <linux/string.h>

// TODO: 修改为你的模块名（必须唯一）
KPM_NAME("kpm-template");

// TODO: 修改版本号
KPM_VERSION("1.0.0");

// TODO: 修改许可证（推荐 GPL v2）
KPM_LICENSE("GPL v2");

// TODO: 修改作者信息
KPM_AUTHOR("Your Name");

// TODO: 修改模块描述
KPM_DESCRIPTION("KPM Template Module");

/**
 * @brief 模块初始化函数
 * @details 在模块加载时调用
 * 
 * @param args 初始化参数
 * @param event 事件类型
 * @param reserved 保留参数
 * @return 0 表示成功，负数表示失败
 */
static long template_init(const char *args, const char *event, void *__user reserved)
{
    pr_info("kpm template init, event: %s, args: %s\n", event, args);
    pr_info("kernelpatch version: %x\n", kpver);
    
    // TODO: 在这里添加你的初始化代码
    
    return 0;
}

/**
 * @brief 控制函数0
 * @details 用于处理用户空间的控制请求
 * 
 * @param args 输入参数
 * @param out_msg 输出消息缓冲区
 * @param outlen 输出缓冲区长度
 * @return 0 表示成功，负数表示失败
 */
static long template_control0(const char *args, char *__user out_msg, int outlen)
{
    pr_info("kpm template control0, args: %s\n", args);
    
    // TODO: 实现你的控制逻辑
    char response[64] = "template response: ";
    strncat(response, args, 40);
    compat_copy_to_user(out_msg, response, sizeof(response));
    
    return 0;
}

/**
 * @brief 控制函数1
 * @details 用于处理带三个参数的控制请求
 * 
 * @param a1 参数1
 * @param a2 参数2
 * @param a3 参数3
 * @return 0 表示成功，负数表示失败
 */
static long template_control1(void *a1, void *a2, void *a3)
{
    pr_info("kpm template control1, a1: %llx, a2: %llx, a3: %llx\n", a1, a2, a3);
    
    // TODO: 实现你的控制逻辑
    
    return 0;
}

/**
 * @brief 模块退出函数
 * @details 在模块卸载时调用
 * 
 * @param reserved 保留参数
 * @return 0 表示成功，负数表示失败
 */
static long template_exit(void *__user reserved)
{
    pr_info("kpm template exit\n");
    
    // TODO: 在这里添加你的清理代码
    
    return 0;
}

// 注册回调函数
KPM_INIT(template_init);
KPM_CTL0(template_control0);
KPM_CTL1(template_control1);
KPM_EXIT(template_exit);

