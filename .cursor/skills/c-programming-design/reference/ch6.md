# 简单控制流原则

*Restrict to Simple Control Flow*

参考来源：
- [🔗 NASA JPL Power of Ten — Rule 1](https://spinroot.com/p10/rule1.html)

### 简单控制流原则

NASA JPL "Power of Ten" 规则第1条要求：**限制所有代码使用简单控制流结构**——禁止使用 goto（除内核集中清理模式）、setjmp/longjmp，以及直接或间接递归。这不是任意限制，而是为了让静态分析工具能够验证程序行为的有界性。

> "Restrict to simple control flow constructs. Do not use goto statements, setjmp or longjmp constructs, and do not use direct or indirect recursion. Simpler control flow translates into stronger capabilities for verification and often results in improved code clarity."
> — — NASA JPL Power of Ten Rules, Rule 1 (Gerard J. Holzmann)

**✅ 设计优势**
- 无递归保证调用图无环，可静态验证栈深度
- 控制流简单的代码更易做形式化验证
- 减少"意外跳转"导致的状态不一致问题
- 更易实现完整的 MC/DC 测试覆盖
- 编译器和分析工具能做更精准的检查
- 航空、航天飞行控制软件
- 医疗设备固件
- 核电站监控软件
- 汽车 ECU 安全功能
- 任何需要 DO-178C 认证的软件

**图7：递归 vs 迭代在安全关键系统中的控制流对比**

- 递归 vs 迭代：安全关键系统的选择
- ❌ 递归（不推荐用于安全关键）
- factorial(n)
- → factorial(n-1)
- → factorial(n-2)
- → ... 栈深度不可预测
- 无法静态验证栈消耗上界
- ✅ 迭代（安全关键首选）
- factorial_iter(n)
- for i in [1..n]
- result *= i;
- 栈深度：O(1)，固定不变
- ✓ 可静态验证，无堆栈溢出风险

### 代码示例对比

**反例 — 递归（安全关键场景）**

```c
/* ❌ NASA JPL 禁止在飞行软件中
   使用递归，栈深不可预测 */
int fib(int n) {
    if (n <= 1) return n;
    return fib(n-1) + fib(n-2);
    /* 双递归：栈深 O(n)，
       n 较大时栈溢出 */
}

int tree_depth(struct node *n) {
    if (!n) return 0;
    /* 树高不可预测 = 栈深不可预测 */
    int l = tree_depth(n->left);
    int r = tree_depth(n->right);
    return 1 + (l > r ? l : r);
}
```

**正例 — 迭代 + 显式栈（有界）**

```c
/* ✅ 正例：迭代实现，栈深 O(1)
   NASA JPL Rule 2: 循环有固定上界 */

#define MAX_NODES 1024

int fib_iter(int n) {
    int a = 0, b = 1, c;
    if (n == 0) return 0;
    /* 固定迭代次数，可静态分析 */
    for (int i = 1; i < n; i++) {
        c = a + b; a = b; b = c;
    }
    return b;
}

int tree_depth_iter(
        struct node *root) {
    /* 显式栈，最大深度有限 */
    struct node *stack[MAX_NODES];
    int depth[MAX_NODES];
    int top = 0, max_d = 0;
    /* BFS/DFS 有界迭代... */
    return max_d;
}
```