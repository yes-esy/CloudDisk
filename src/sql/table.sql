CREATE TABLE t_user (
    id INT AUTO_INCREMENT PRIMARY KEY COMMENT '用户 id,主键',
    username VARCHAR(255) NOT NULL COMMENT '用户名',
    salt CHAR(32) COMMENT '盐值',
    cryptpasswd VARCHAR(255) NOT NULL COMMENT '加密密码',
    pwd VARCHAR(255) COMMENT '当前工作目录'
)COMMENT='用户表';

-- 假设最多 5 层目录
WITH RECURSIVE path_tree AS (
  SELECT id, parent_id, filename, type, 0 AS depth
  FROM virtual_fs
  WHERE parent_id = 0 AND owner_id = 1001 AND filename = 'a'

  UNION ALL

  SELECT v.id, v.parent_id, v.filename, v.type, pt.depth + 1
  FROM virtual_fs v
  JOIN path_tree pt ON v.parent_id = pt.id
  WHERE v.owner_id = 1001
)
SELECT id, md5, filesize
FROM path_tree
WHERE filename = 'report.pdf' AND type = 1;