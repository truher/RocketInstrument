import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import sys

if len(sys.argv) < 2:
  print("provide number arg")
  quit()

num = sys.argv[1]
print(f'loading file {num}.')

f, ax=plt.subplots(3,1,sharex=True)

df = pd.read_csv(f'kx.{num}.txt', sep='\t', names=['t','ax','ay','az'], engine='c')
df['td'] = df['t'].diff().fillna(0)

ax[0].scatter(df['t']/1e6, df['az'], s=1)
ax[0].set_title('accelerometer')
ax[0].set_ylabel('acceleration (?)')

dfb = pd.read_csv(f'bmp.{num}.txt', sep='\t', names=['t','T','p','a'], engine='c')
dfb['td'] = dfb['t'].diff().fillna(0)

ax[1].scatter(dfb['t']/1e6, dfb['a'], s=1)
ax[1].set_title('pressure/temperature')
ax[1].set_ylabel('altitude (m)')

dfi = pd.read_csv(f'imu.{num}.txt', sep='\t', names=['t','ax','ay','az','gx','gy','gz','mx','my','mz','T'], engine='c')
dfi['td'] = dfi['t'].diff().fillna(0)

ax[2].scatter(dfi['t']/1e6, dfi['az'], s=1)
ax[2].set_title('imu')
ax[2].set_xlabel('time (s)')
ax[2].set_ylabel('acceleration (?)')


plt.show()
