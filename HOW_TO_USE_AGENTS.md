协作工作流 (Workflow)
为了最大化学习效果，建议按照以下流程组合使用 Agent：

项目启动阶段：

呼叫 Agent D (领航员) 明确本月目标和关键知识点。

呼叫 Agent B (架构师) 讨论实战项目的整体设计结构（类图、模块划分）。

核心开发阶段：

使用 Agent A (性能宗师) 辅助编写核心 C++ 逻辑，解决并发难题。

如果是 AI 相关模块（第 7-12 月），切换至 Agent C (AI 工程师) 解决数学和算子实现问题。

代码审查与优化阶段：

将写好的代码投喂给 Agent A，要求其进行 "Code Review for Performance"。

询问 Agent A："这段代码在 L3 Cache 命中率上有什么问题？"

复盘与验收阶段：

呼叫 Agent D 进行阶段性 Mock Interview。

询问 Agent B："目前的实战项目在工业界标准看有什么缺陷？"

快速指令示例 (Prompt Snippets)
给 Agent A (C++): "Review my thread pool implementation based on C++17 memory model. Check for potential deadlocks and false sharing."

给 Agent B (Arch): "I'm designing the C10k HTTP server. Compare Reactor (epoll) vs Proactor (io_uring) for my use case. Give me a class diagram."

给 Agent C (AI): "Explain how Self-Attention is implemented in C++ purely. How do we optimize the Softmax calculation using AVX2?"

给 Agent D (Coach): "I'm stuck on Month 5 (Distributed KV Store). Break down the Raft implementation into 4 smaller, manageable weekly tasks."