#ifndef AGENT_LOOP_H
#define AGENT_LOOP_H

/*
 * 交互式 REPL: 读用户输入, 驱动 agent 内层循环 (模型↔工具), 直到 EOF/exit/quit。
 * initial 非 NULL 时作为首条 prompt (来自命令行), 之后转入交互读取。
 */
void agent_loop_run(const char *initial);

#endif /* AGENT_LOOP_H */
