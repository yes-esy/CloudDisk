CREATE TABLE t_user (
    id INT AUTO_INCREMENT PRIMARY KEY COMMENT '用户 id,主键',
    username VARCHAR(255) NOT NULL COMMENT '用户名',
    salt CHAR(32) COMMENT '盐值',
    cryptpasswd VARCHAR(255) NOT NULL COMMENT '加密密码',
    pwd VARCHAR(255) COMMENT '当前工作目录'
)COMMENT='用户表';