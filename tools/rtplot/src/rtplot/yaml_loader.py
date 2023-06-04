import yaml
from dataclasses import dataclass


@dataclass
class TenokMsgField:
    c_type: bool
    var_name: str
    array_size: int
    description: str


@dataclass
class TenokMsg:
    name: str
    msg_id: int
    fields: list


class TenokMsgManager:
    def __init__(self):
        self.msg_list = []

    def load(self, msg_file_path, msg_name):
        with open(msg_file_path, "r") as stream:
            data = yaml.load(stream, Loader=yaml.FullLoader)

        msg = TenokMsg(msg_name, data['msg_id'], [])

        for i in range(0, len(data['payload'])):
            m = TenokMsgField(data['payload'][i]['c_type'],
                              data['payload'][i]['var_name'],
                              data['payload'][i]['array_size'],
                              data['payload'][i]['description'])
            msg.fields.append(m)

        self.msg_list.append(msg)

    def find(self, msg_name):
        for i in range(0, len(self.msg_list)):
            if self.msg_list[i].name == msg_name:
                return self.msg_list[i]
