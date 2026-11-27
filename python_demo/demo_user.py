"""Reuse User commands through an interactive prompt or a single command."""

import argparse
import shlex
import sys

from aheadform_sdk import AheadFormSDK, DEFAULT_CONFIG, RET_SUCCESS, RET_SKIPPED, RET_EXIT


def _report_result(code):
    if code not in (RET_SUCCESS, RET_SKIPPED, RET_EXIT):
        print("User 返回 0x{:08X}".format(code & 0xFFFFFFFF), file=sys.stderr)
        return 1
    return 0


def _interactive(sdk):
    try:
        sdk.open()
        print("输入 help 查看指令，exit 退出；Ctrl+C 或 Ctrl+D 结束。")
        commands = {command.lower() for command in sdk.commands()}
        while True:
            try:
                line = input(">> ")
            except EOFError:
                print()
                return 0
            try:
                command = shlex.split(line)
            except ValueError as error:
                print("输入无效：" + str(error), file=sys.stderr)
                continue
            if not command:
                continue
            name = command[0].lower()
            if name not in commands:
                print("未知指令：{}；输入 help 查看指令。".format(command[0]), file=sys.stderr)
                continue
            try:
                code = sdk.execute(command[0], *command[1:], report_result=True)
            except ValueError as error:
                print(str(error), file=sys.stderr)
                continue
            _report_result(code)
            if code == RET_EXIT:
                return 0
    except KeyboardInterrupt:
        print()
        return 130
    finally:
        sdk.close()


def main():
    parser = argparse.ArgumentParser(description="复用 User 菜单业务；无指令时进入交互模式")
    parser.add_argument("--config", default=str(DEFAULT_CONFIG), help="SDK INI 配置文件")
    parser.add_argument("--list", action="store_true", help="列出已注册指令，不初始化机器人")
    parser.add_argument("command", nargs=argparse.REMAINDER, help="指令名及其参数；不填则连续输入")
    args = parser.parse_args()
    try:
        sdk = AheadFormSDK(config=args.config, use_user=True)
        if args.list:
            for command in sdk.commands():
                print(command)
            return 0
        if not args.command:
            return _interactive(sdk)
        command = args.command
        if command[0].lower() in ("help", "exit"):
            code = sdk.execute(command[0], *command[1:], report_result=True)
        else:
            with sdk:
                code = sdk.execute(command[0], *command[1:], report_result=True)
        return _report_result(code)
    except KeyboardInterrupt:
        print()
        return 130
    except (OSError, RuntimeError, ValueError) as error:
        print(str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
