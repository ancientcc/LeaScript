package org.libsdl.app;

import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;

public class SchedulePowerOnOffUtil {

    public final static String ACTION_SCHEDULE_POWER_ON = "android.fapi.action.run_power_on";
    public final static String ACTION_SCHEDULE_POWER_OFF = "android.fapi.action.run_power_off";
    public final static String ACTION_SCHEDULE_POWER_REBOOT = "android.fapi.action.run_power_reboot";

    /***
     * 设置定时开机,id由用户定义，用于开启和关闭定时开机时使用，重启后失效需要重新设置
     * @param id　　　　　　　　　
     * @param enabled　　　　开启/关闭
     * @param alarm_time　开机时间(UTC时间)
     */
    public static void setSchedulePowerOn(Context context , int id, boolean enabled, long alarm_time){
		AlarmManager alarmManager = (AlarmManager)context.getSystemService(Context.ALARM_SERVICE);
		if(enabled)
		{
			alarmManager.setExact(/*AlarmManager.BOOT*/4,  alarm_time, getPendingIntent(context,ACTION_SCHEDULE_POWER_ON,id));
		}else{
			alarmManager.cancel(getPendingIntent(context,ACTION_SCHEDULE_POWER_ON,id));
		}

    }

    /***
     * 设置定时关机,id由用户定义，用于开启和关闭定时关机时使用，重启后失效需要重新设置
     * @param id　　　　　　　　　
     * @param enabled　　　　开启/关闭
     * @param alarm_time　开机时间(UTC时间)
     */
    public static void setSchedulePowerOff(Context context,int id,boolean enabled,long alarm_time){
		AlarmManager alarmManager = (AlarmManager)context.getSystemService(Context.ALARM_SERVICE);
		if(enabled)
		{
			alarmManager.setExact(/*AlarmManager.RTC_WAKEUP*/0,  alarm_time, getPendingIntent(context,ACTION_SCHEDULE_POWER_OFF,id));
		}else{
			alarmManager.cancel(getPendingIntent(context,ACTION_SCHEDULE_POWER_OFF,id));
		}
    }

    /***
     * 设置定时重启,id由用户定义，用于开启和关闭定时重启时使用，重启后失效需要重新设置
     * @param id　　　　　　　　　
     * @param enabled　　　　开启/关闭
     * @param alarm_time　开机时间(UTC时间)
     */
    public static void setSchedulePowerReboot(Context context,int id,boolean enabled,long alarm_time){
        AlarmManager alarmManager = (AlarmManager)context.getSystemService(Context.ALARM_SERVICE);
        if(enabled)
        {
            alarmManager.setExact(/*AlarmManager.RTC_WAKEUP*/0,  alarm_time, getPendingIntent(context,ACTION_SCHEDULE_POWER_REBOOT,id));
        }else{
            alarmManager.cancel(getPendingIntent(context,ACTION_SCHEDULE_POWER_REBOOT,id));
        }

    }


    private static PendingIntent getPendingIntent(Context context, String action, int requestCode)
    {
        Intent intent = new Intent();
        intent.setClassName("com.firefly.fireflyapi2service","com.firefly.fireflyapi2service.InitReceiver");
        intent.setAction(action);
        PendingIntent pendingIntent = PendingIntent.getBroadcast(context, requestCode, intent,PendingIntent.FLAG_IMMUTABLE |PendingIntent.FLAG_UPDATE_CURRENT);
        return pendingIntent;

    }
}
