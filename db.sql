drop database if exists cc_gobang;
create database if not exists cc_gobang;
use cc_gobang;
create table if not exists user(
    id int primary key auto_increment,
    username varchar(32) unique key not null,
    password varchar(128) not null,
    score int,
    total_count int,
    win_count int,
    photo varchar(100) not null
);

insert into user value(null, 'zhangsan', md5('111'), 500, 0, 0, 'mk1WRFta-TnFo-CqKW-Ev6v-FN1j4bMPIRDzX71D.jpg');
insert into user value(null, 'lisi', md5('111'), 500, 0, 0, 'vmYnrZuN-jOXs-aDHQ-f5s1-1JXPy27zbh7MStQj.jpeg');
insert into user value(null, 'wangwu', md5('111'), 500, 0, 0, 'waiting.jpg');
insert into user value(null, 'zhaoliu', md5('111'), 500, 0, 0, 'waiting.jpg');
insert into user value(null, 'wangwuzhang', md5('111'), 500, 0, 0, 'waiting.jpg');
insert into user value(null, 'zhangsanwangwu', md5('111'), 500, 0, 0, 'waiting.jpg');

create table if not exists friends(
    uid int,
    friend_id int,
    statu int default 1
);

insert into friends value(1, 2, 1);
insert into friends value(2, 1, 1);
insert into friends value(1, 3, 1);
insert into friends value(3, 1, 1);