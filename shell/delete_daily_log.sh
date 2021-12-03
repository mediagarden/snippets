#!/bin/bash
# 删除七天以前的日志
log_dir=/bh/logs
find $log_dir -type f -mtime +7 | xargs rm -rvf

# 删除修改时间发生了变化的日志
for file in $log_dir/*; do
    #echo $file
    log_file=${file##*/}
    #echo $log_file
    is_match=`expr match $log_file "^daily_.*.log$"`
    if [ $is_match -gt 0 ];then
		#echo "it is log file:"$file
		file_date_str=${log_file#*_}
		file_date_str=${file_date_str%%.*}
		file_date=`date -d ${file_date_str}`
		now_date=`date`

		file_seconds=$(date --date="$file_date" +%s);
		now_seconds=$(date --date="$now_date" +%s);
		days=$((($now_seconds-$file_seconds)/86400))
		#echo "file create $days days"
		if [ $days -gt 7 ];then
			echo "delete file: "$file
			rm -rf $file
		fi
    fi
done
