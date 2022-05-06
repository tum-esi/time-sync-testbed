# Copyright (c) Thomas Wakim, 2022
#     t7wakim@gmail.com

import yaml
import pprint
from . import logger


def parse_yaml(file_name):
    with open(file_name, "r") as stream:
        try:
            config = yaml.safe_load(stream)
            logger.debug(f"Loaded YAML: {file_name}\n" + pprint.pformat(config))
        except yaml.YAMLError as exc:
            logger.error(exc)
    return config
