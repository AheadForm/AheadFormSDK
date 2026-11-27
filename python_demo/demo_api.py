"""Read-only by default. Control operations require explicit command-line options."""

import argparse
from dataclasses import asdict
import json
import sys

from aheadform_sdk import AheadFormSDK, DEFAULT_CONFIG


def main():
    parser = argparse.ArgumentParser(description="调用现有 C API；默认仅查询，不发送动作")
    parser.add_argument("--config", default=str(DEFAULT_CONFIG), help="SDK INI 配置文件")
    parser.add_argument("--query", choices=("volume", "mode", "neck", "all"), default="all")
    parser.add_argument("--set-mode", choices=("dev", "interactive"), help="明确指定才切换运行模式")
    parser.add_argument("--neck", nargs=3, type=float, metavar=("PITCH", "YAW", "ROLL"), help="明确指定才发送一帧颈部姿态，单位 rad")
    args = parser.parse_args()
    try:
        with AheadFormSDK(config=args.config) as sdk:
            if args.set_mode is not None:
                sdk.set_operating_mode(args.set_mode)
            if args.neck is not None:
                sdk.send_neck_pose(*args.neck)
            if args.query in ("volume", "all"):
                print("音量:", asdict(sdk.query_volume()))
            if args.query in ("mode", "all"):
                print("运行模式:", sdk.get_operating_mode().name)
            if args.query in ("neck", "all"):
                state = sdk.read_latest_neck_state()
                print("颈部状态:", "暂无缓存" if state is None else json.dumps(asdict(state), ensure_ascii=False))
    except (OSError, RuntimeError, ValueError, KeyError) as error:
        print(str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
