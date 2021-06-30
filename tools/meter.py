import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import sys

if len(sys.argv) < 2:
  print("provide number arg")
  quit()

num = sys.argv[1]
print(f'loading file {num}.')

f, ax=plt.subplots(4,1,sharex=True)

df = pd.read_csv(f'kx.{num}.txt', sep='\t', names=['t','ax','ay','az'], engine='c')
# dt in microseconds
df['td'] = df['t'].diff().fillna(0)
df['g'] = np.sqrt(df['ax']*df['ax']+df['ay']*df['ay']+df['az']*df['az'])
mean_z = df['az'].mean()
print(mean_z)
df['az_cal'] = df['az'] - (mean_z - 1.0)
print(df['az_cal'].mean())
# accel z net of gravity in m/s^2
df['azn'] = (df['az_cal'] - 1.0) * 9.8
# dv z in m/s/s
df['dvz'] = df['azn'] * df['td'] / 1.0e6
# v z in m/s
df['vz'] = df['dvz'].cumsum()
df['dpz'] = df['vz'] * df['td'] / 1.0e6
df['pz'] = df['dpz'].cumsum()

ax[0].scatter(df['t']/1e6, df['az_cal'], s=1)
ax[0].set_title('accelerometer')
ax[0].set_ylabel('acceleration (m/s/s)')

ax[1].scatter(df['t']/1e6, df['vz'], s=1)
ax[1].set_title('accelerometer')
ax[1].set_ylabel('velocity (m/s)')

ax[2].scatter(df['t']/1e6, df['pz'], s=1)
ax[2].set_title('accelerometer')
ax[2].set_ylabel('position (m)')

dfb = pd.read_csv(f'bmp.{num}.txt', sep='\t', names=['t','T','p','a'], engine='c')
dfb['td'] = dfb['t'].diff().fillna(0)

ax[3].scatter(dfb['t']/1e6, dfb['a'], s=1)
ax[3].set_title('pressure/temperature')
ax[3].set_xlabel('time (s)')
ax[3].set_ylabel('altitude (m)')

plt.show()
