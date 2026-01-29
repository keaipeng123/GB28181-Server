# PJSIP 业务任务基类（`SipTaskBase`）笔记

> 对应工程代码（上下级两套服务一致）：
>
> - `SipSubService/include/SipTaskBase.h`
> - `SipSubService/src/SipTaskBase.cpp`
> - `SipSupService/include/SipTaskBase.h`
> - `SipSupService/src/SipTaskBase.cpp`
>
> 目标：搞清楚这个“父级基础类”在 PJSIP 业务里的定位、派生类怎么写、以及两个通用解析函数（From / XML）的行为与坑。

---

## 1. `SipTaskBase` 在工程中的定位

`SipTaskBase` 是一个 **PJSIP 业务处理的抽象基类**，用于统一“收到 SIP 报文 → 解析必要字段 → 执行业务逻辑”的入口。

它提供：

- 一个统一的业务入口：`run(pjsip_rx_data* rdata)`（纯虚函数，派生类必须实现）
- 两个“通用解析能力”（不依赖具体业务）：
  - `parseFromId(...)`：从 SIP 的 `From` 头里抽取本工程约定的 ID
  - `parseXmlData(...)`：从 SIP 消息 body 里解析 XML，获取根节点类型与某个指定标签值

这种设计的意图：

- 上层模块（例如接收模块 `recv_mod` 或分发器）可以只面对 `SipTaskBase*`，而不关心具体业务类。
- 每个业务类只需专注实现自己的 `run()`。

---

## 2. 类接口一览（你需要记住的“最少 API”）

头文件核心接口：

```cpp
class SipTaskBase {
public:
    SipTaskBase() {}
    virtual ~SipTaskBase() {
        LOG(INFO) << "~SipTaskBase";
    }

    virtual pj_status_t run(pjsip_rx_data *rdata) = 0;

    static tinyxml2::XMLElement* parseXmlData(
        pjsip_msg* msg,
        std::string& rootType,
        const std::string xmlkey,
        std::string& xmlvalue);

protected:
    std::string parseFromId(pjsip_msg* msg);
};
```

要点：

- **虚析构**：确保你用基类指针删除派生类对象时，派生类析构能被调用（避免资源泄漏）。
- **`run` 纯虚函数**：把“业务处理”强制留给派生类。
- `parseFromId` 放在 `protected`：只允许派生类复用，不鼓励外部直接调用。
- `parseXmlData` 是 `static`：不需要对象状态即可使用，适合通用工具函数。

---

## 3. `run(pjsip_rx_data* rdata)`：派生类怎么用

### 3.1 典型调用链（逻辑视图）

1) PJSIP 收到报文，进入你的接收回调（例如 `on_rx_request`）
2) 业务分发：根据 `Method` / `CmdType` / `EventType` 等字段选择一个具体任务类
3) 调用 `task->run(rdata)` 执行业务处理

### 3.2 派生类实现模板（示意）

```cpp
class CatalogTask : public SipTaskBase {
public:
    pj_status_t run(pjsip_rx_data* rdata) override {
        if (!rdata || !rdata->msg_info.msg) {
            return PJ_EINVAL;
        }

        pjsip_msg* msg = rdata->msg_info.msg;

        std::string fromId = parseFromId(msg);

        std::string rootType;
        std::string sn;
        tinyxml2::XMLElement* root = parseXmlData(msg, rootType, "SN", sn);

        // TODO: 结合 rootType / sn / fromId 做业务处理
        return PJ_SUCCESS;
    }
};
```

注意：上面只是“结构模板”，具体的分发规则与业务字段以工程实际为准。

---

## 4. `parseFromId(pjsip_msg* msg)`：从 `From:` 抽取 ID

实现逻辑（简化描述）：

1) 用 `pjsip_msg_find_hdr` 找 `PJSIP_H_FROM`
2) 用 `print_on` 把 `From` 头打印成字符串
3) 对字符串进行固定位置截取：`substr(11, 20)`

工程注释示例：

```text
From: <sip:130909113319427420@10.64.49.218:7100>;tag=...
```

### 4.1 这个截取规则的含义

`substr(11, 20)` 等价于假设：

- `From:` 开头格式固定
- `<sip:` 后面的“ID 长度固定为 20”
- 并且 `<sip:` 在字符串中的位置固定

因此它能在你当前示例里提取到：

- `130909113319427420`（20 位 ID）

### 4.2 风险点（写业务时要知道）

- 报文格式略有变化（空格、大小写、是否带 display-name、ID 长度变化）就可能截错。
- `substr(11, 20)` 没有做边界检查，字符串短时可能抛异常/产生未定义行为（取决于实现与编译选项）。

更稳妥的思路（后续工程化再做）：

- 从 `pjsip_from_hdr` 的 URI 结构里解析 user 部分，而不是对 `print_on` 的字符串做硬编码截取。

---

## 5. `parseXmlData(...)`：从 body 解析 XML 并抽取字段

当前实现行为：

1) `new tinyxml2::XMLDocument()`
2) `pxmlDoc->Parse((char*)msg->body->data)`
3) `RootElement()` 作为根节点
4) `rootType = root->Value()`（根节点标签名）
5) 找到 `xmlkey` 对应的子节点，取文本写入 `xmlvalue`
6) 返回根节点指针

这让派生类可以快速得到两个常见信息：

- XML 根节点类型（例如 `Query/Response/Notify` 等，取决于 GB28181 的具体消息）
- 指定标签的值（例如 `SN`、`CmdType`、`DeviceID` 等）

### 5.1 必须知道的坑：`XMLDocument` 生命周期与内存泄漏

当前实现：

- `XMLDocument` 用 `new` 创建，但 **没有 `delete`** → **内存泄漏**
- 返回的 `XMLElement*` 指针依赖 `XMLDocument` 的内部存储
  - 如果你把 `XMLDocument` 释放了，`XMLElement*` 会立刻悬空
  - 现在之所以“没炸”，是因为 `XMLDocument` 泄漏导致它一直活着

更合理的接口设计（思路）：

- 让调用者提供 `tinyxml2::XMLDocument&`（由调用者管理生命周期）
- 或返回 `std::unique_ptr<tinyxml2::XMLDocument>`，并从中取 root

### 5.2 边界检查（建议派生类自己也要防御）

当前实现没有检查：

- `msg == nullptr`
- `msg->body == nullptr`
- `msg->body->data == nullptr`
- `pxmlDoc->Parse(...)` 返回值是否成功
- `RootElement() == nullptr`

因此派生类在调用它前后，最好把 `msg/body` 等做基本判空，并对解析失败做降级处理。

---

## 6. 小结：如何在项目里正确“继承并使用”

- `SipTaskBase` = 统一业务入口（`run`） + 两个通用解析工具（From / XML）。
- 派生类只做三件事：
  1) 从 `rdata` 取 `pjsip_msg*`
  2) 复用 `parseFromId/parseXmlData` 拿业务需要的字段
  3) 执行业务逻辑并返回 `pj_status_t`
- 现有 `parseXmlData` 的实现存在明显的生命周期/泄漏问题：学习阶段可以先理解其行为，工程化阶段建议重构接口，把 `XMLDocument` 生命周期交回调用者管理。
