moveit.urdf
相比官方的，删除：
left_front_wheel_joint、left_rear_wheel_joint、right_front_wheel_joint、right_rear_wheel_joint、
joint7、joint8、joint9、joint10、joint11。
相关地删除left_front_wheel_link(left_front_wheel_joint的child)、left_rear_wheel_link（left_rear_wheel_joint的child）、right_front_wheel_link（right_front_wheel_joint的child），right_rear_wheel_link（right_rear_wheel_joint的child），link7（joint7的child）、link8（joint8的child）、link9（joint9的child）、link10（joint10的child）、link11（joint11的child）

joint2
limit.upper="1.57"改为"0.76"。原因：>0时，机械臂要向后倒，这很快就会碰到后面的雷达，但有时必须向后倒，给上0.76。

joint3
limit.lower="-1.57"改为"0"。原因：<0时，机械臂要向后倒，这相机支架极可能就会碰到后端的机械臂。

joint4
limit.lower="-0.8"改为"0"。原因：<0时，机械臂要向后倒，这相机支架极可能就会碰到后端的机械臂。

moveit.srdf
相比官方的，“handle”组只留joint6。删除link7、link8、link9、link10、link11