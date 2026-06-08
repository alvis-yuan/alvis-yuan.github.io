---
name: c-programming-design
description: Guides C language software architecture and GoF design patterns in user-space/system code — layered modules, error handling, ownership, event-driven design, and pattern selection (Singleton, Factory, Observer, Strategy, etc.). Use when designing C modules, choosing architecture vs patterns, reviewing structure, or when the user mentions C architecture, design patterns, layering, or module boundaries.
---

# C 语言架构设计

自包含的 C 语言**架构原则**（第 0–10 章）与 **GoF 设计模式**（第 11–36 章）指南。参考 Linux 内核、SQLite、Redis 等实践。编写或审查 C 架构时，**必须先阅读相关 reference 章节**。

## 使用方式

1. **先架构、后模式**：模块划分、接口、所有权、错误策略未定时，读第 1–10 章；具体协作/创建/结构问题再读第 11–36 章。
2. 模式选型使用 [第 36 章 设计模式决策树](reference/ch36.md)。
3. 编码细节（命名、类型、RAII、编译期检查）见 **c-programming-spec** skill；本 skill 聚焦**结构与模式**。
4. 每条含 **文字说明** 与 **正/反面代码示例**（源文档提供时保留）。

## 章节索引

- [总览与选型](reference/ch0.md)
- [第1章 单一职责与函数短小原则](reference/ch1.md)
- [第2章 分层架构与模块化设计](reference/ch2.md)
- [第3章 抽象接口与信息隐藏](reference/ch3.md)
- [第4章 集中式错误处理 (goto 清理模式)](reference/ch4.md)
- [第5章 数据驱动设计](reference/ch5.md)
- [第6章 简单控制流原则](reference/ch6.md)
- [第7章 防御性编程与断言设计](reference/ch7.md)
- [第8章 可移植性设计原则](reference/ch8.md)
- [第9章 内存所有权语义](reference/ch9.md)
- [第10章 事件驱动架构与单线程模型](reference/ch10.md)
- [第11章 设计模式概述](reference/ch11.md)
- [第12章 23 种设计模式总览](reference/ch12.md)
- [第13章 单例模式（Singleton）](reference/ch13.md)
- [第14章 工厂方法模式（Factory Method）](reference/ch14.md)
- [第15章 抽象工厂模式（Abstract Factory）](reference/ch15.md)
- [第16章 建造者模式（Builder）](reference/ch16.md)
- [第17章 原型模式（Prototype）](reference/ch17.md)
- [第18章 适配器模式（Adapter）](reference/ch18.md)
- [第19章 桥接模式（Bridge）](reference/ch19.md)
- [第20章 组合模式（Composite）](reference/ch20.md)
- [第21章 装饰器模式（Decorator）](reference/ch21.md)
- [第22章 外观模式（Facade）](reference/ch22.md)
- [第23章 享元模式（Flyweight）](reference/ch23.md)
- [第24章 代理模式（Proxy）](reference/ch24.md)
- [第25章 观察者模式（Observer）](reference/ch25.md)
- [第26章 策略模式（Strategy）](reference/ch26.md)
- [第27章 命令模式（Command）](reference/ch27.md)
- [第28章 责任链模式（Chain of Responsibility）](reference/ch28.md)
- [第29章 状态模式（State）](reference/ch29.md)
- [第30章 模板方法模式（Template Method）](reference/ch30.md)
- [第31章 迭代器模式（Iterator）](reference/ch31.md)
- [第32章 中介者模式（Mediator）](reference/ch32.md)
- [第33章 访问者模式（Visitor）](reference/ch33.md)
- [第34章 备忘录模式（Memento）](reference/ch34.md)
- [第35章 解释器模式（Interpreter）](reference/ch35.md)
- [第36章 设计模式决策树](reference/ch36.md)
- [权威参考来源](reference/ch37.md)

## 快速检查清单

- [ ] 模块职责单一、函数短小（第 1 章）
- [ ] 分层清晰，层间仅通过头文件接口通信（第 2 章）
- [ ] 对外暴露 opaque 类型 / 最小 API（第 3 章）
- [ ] 多 exit 路径有集中错误处理或 RAII（第 4 章；编码见 c-programming-spec）
- [ ] 表驱动 / 数据与逻辑分离（第 5 章）
- [ ] 控制流扁平，圈复杂度可控（第 6 章）
- [ ] 内存所有权在 API 文档与实现中一致（第 9 章）
- [ ] 需要模式时对照决策树，避免过度设计（第 36 章）

## 权威参考

完整外部链接见 [reference/ch37.md](reference/ch37.md)。
