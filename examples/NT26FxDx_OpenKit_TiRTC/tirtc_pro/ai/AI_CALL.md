# AI 呼叫配置入口

完整说明已统一到 [AI 呼叫微信与设备配置详解](../docs/AI呼叫微信与设备配置详解.md)，此处保留旧链接入口。

- 设备插件 Action：`call_contact`；target 为完整联系人名称或 ID。
- 普通呼叫默认 `video`；明确只要语音时传 `audio`。
- 固件不会自动修改云端角色；角色、插件、参数默认值和设备关联都需生效。
- 先验证通讯录手动呼叫，再验证 AI 的真实拨号和接听。

实现入口是 `tirtc_ai.c`、`ai_call_protocol.c` 和原 contacts/calls 模块；当前功能与验证入口见 [AI 模块说明](README.md)。
