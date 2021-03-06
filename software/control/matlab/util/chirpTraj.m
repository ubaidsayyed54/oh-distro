function traj = chirpTraj(amp,min_freq,max_freq,T,offset,sign)

if nargin < 5
  offset = 0; 
end
if nargin < 6
  sign = 0; % sin wave centered at offset
end

ts = 0:0.05:T;

freq = linspace(min_freq,max_freq,length(ts));% <----  cycles per second

if sign == 0
  traj = PPTrajectory(foh(ts, offset + amp*sin(ts.*freq*2*pi)));
else
  traj = PPTrajectory(foh(ts, offset + sign*(0.5*amp - 0.5*amp*cos(ts.*freq*2*pi))));
end
