import json,os,sys,time
p=sys.argv[1]
def read():
 d=open('/proc/'+p+'/stat').read().split(); s=dict(line.split(':',1) for line in open('/proc/'+p+'/status'))
 return {'cpu_ticks':int(d[13])+int(d[14]),'vm_rss':s.get('VmRSS','').strip(),'vm_hwm':s.get('VmHWM','').strip()}
a=read();time.sleep(2);b=read();print(json.dumps({'pid':int(p),'observation_seconds':2,'clock_ticks_per_second':os.sysconf('SC_CLK_TCK'),'before':a,'after':b,'cpu_ticks_delta':b['cpu_ticks']-a['cpu_ticks'],'limits':'One 2-second idle Linux X11 cloud-window sample; not energy or battery measurement.'},indent=2))
