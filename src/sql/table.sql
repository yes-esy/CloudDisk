CREATE TABLE t_user (
    id INT AUTO_INCREMENT PRIMARY KEY COMMENT '用户 id,主键',
    username VARCHAR(255) NOT NULL COMMENT '用户名',
    salt CHAR(32) COMMENT '盐值',
    cryptpasswd VARCHAR(255) NOT NULL COMMENT '加密密码',
    pwd VARCHAR(255) COMMENT '当前工作目录'
)COMMENT='用户表';
CREATE TABLE virtual_fs (
    id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY COMMENT '主键ID',
    parent_id INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '父目录ID，0表示根目录',
    filename VARCHAR(255) NOT NULL COMMENT '文件或文件夹名称',
    owner_id INT UNSIGNED NOT NULL COMMENT '所属用户ID',
    md5 CHAR(32) NULL COMMENT '文件MD5值（仅文件有效）',
    file_size BIGINT UNSIGNED NULL DEFAULT 0 COMMENT '文件大小（字节，仅文件有效）',
    type TINYINT UNSIGNED NOT NULL DEFAULT 1 COMMENT '类型：1=文件，2=文件夹',
    is_deleted TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '逻辑删除标识：0=正常，1=回收站',
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,

    -- 核心业务索引：防止重名 + 快速列出目录
    UNIQUE KEY uk_owner_parent_name (owner_id, parent_id, filename),
    
    -- 秒传/查重索引
    INDEX idx_md5 (md5),
    
    -- 全局搜索索引（可选）
    INDEX idx_owner_filename (owner_id, filename),
    
    -- （可选）空间统计优化索引
    INDEX idx_owner_size (owner_id, file_size)

) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='虚拟文件系统表';
