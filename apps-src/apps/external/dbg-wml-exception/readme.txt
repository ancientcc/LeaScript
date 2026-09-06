做了个隔1分钟变量触发。执行的是个cpp任务，当中有个单任务是parse_time。

可以看到，此时aplt_task还是个parse_time，可又来request_single_task，参数req_task也是parse_time。



1、检查函数栈，是否有发生tros_cpp_api::slice()重入。
2、查看上面较完下整log。通过log恢复出执行逻辑。
通以下两次出现失败时实例，因而是放置网格失败，导致系统异常，然后再导致这个莫名其妙错误。

---以下是一个出问题时现场：出现了放置网格失败，抛出异常。此时界面正开着“中心”窗口，且没进入“相机”。
10小时10分13秒 It is time to execute: aplt.launcher.fake-sitting_correcio
{task_cpp}[tbg_task::shedule]shedule one 'cpp' task run
567535 go_to_state(state: 0, threshold_s: 60, is_aplt_task: true) scene: 2
call trunning.clear
放置网格(<format>text="--" color="255, 255, 0, 0"</format>)失败！该网格的父控件：<format>text="_chat_history_cell" color="255, 255, 0, 0"</format>

根据标称尺寸和放大系数计算出的渲染尺寸：<format>text="(1095,142)" color="255, 255, 0, 0"</format>
实时计算出的标称尺寸：<format>text="(522,184)" color="255, 255, 0, 0"</format>

如果放置失败网格是stack控件的某个grid，请确保stack的渲染尺寸>=该grid的标称尺寸
Exception thrown at 0x00007FFD8EBE5339 in launcher.exe: Microsoft C++ exception: gui2::twindow::tlayout_exception at memory location 0x0000005FA53B7E20.
twindow::show, catch unknown exception, throw
Exception thrown at 0x00007FFD8EBE5339 in launcher.exe: Microsoft C++ exception: [rethrow] at memory location 0x0000000000000000.
Exception thrown at 0x00007FFD8EBE5339 in launcher.exe: Microsoft C++ exception: gui2::twindow::tlayout_exception at memory location 0x0000005FA53B7E20.
567561 {dbg_scroll}twindow::invalidate_layout(widget: nullptr)
567562 go_to_state(state: 0, threshold_s: 60, is_aplt_task: true) scene: 2


---以下是又一个出问题时现场：出现了放置网格失败，抛出异常。此时界面正开着“中心”窗口，且没进入“相机”。
15小时59分47秒 It is time to execute: aplt.launcher.fake-sitting_correcio
{task_cpp}[tbg_task::shedule]shedule one 'cpp' task run
{dbg_align_item_position}(1)item_position(10521) + content_rect.h(261) > content_grid_rect.h(13795), nothing
12473 go_to_state(state: 0, threshold_s: 60, is_aplt_task: true) scene: 2
call trunning.clear
放置网格(<format>text="--" color="255, 255, 0, 0"</format>)失败！该网格的父控件：<format>text="_chat_history_cell" color="255, 255, 0, 0"</format>

根据标称尺寸和放大系数计算出的渲染尺寸：<format>text="(685,87)" color="255, 255, 0, 0"</format>
实时计算出的标称尺寸：<format>text="(304,109)" color="255, 255, 0, 0"</format>

如果放置失败网格是stack控件的某个grid，请确保stack的渲染尺寸>=该grid的标称尺寸
Exception thrown at 0x00007FFD28BBA80A in launcher.exe: Microsoft C++ exception: gui2::twindow::tlayout_exception at memory location 0x000000114BCF6A50.
twindow::show, catch unknown exception, throw
Exception thrown at 0x00007FFD28BBA80A in launcher.exe: Microsoft C++ exception: [rethrow] at memory location 0x0000000000000000.
Exception thrown at 0x00007FFD28BBA80A in launcher.exe: Microsoft C++ exception: gui2::twindow::tlayout_exception at memory location 0x000000114BCF6A50.
12487 go_to_state(state: 0, threshold_s: 60, is_aplt_task: true) scene: 2


