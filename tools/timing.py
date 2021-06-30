import pandas as pd
import matplotlib.pyplot as plt
import sys

if len(sys.argv) < 2:
  print("provide number arg")
  quit()

num = sys.argv[1]
print(f'loading file {num}.')

f, ax=plt.subplots(3,1,sharex=True)

df = pd.read_csv(f'kx.{num}.txt', sep='\t', names=['t','x','y','z'], engine='c')
df['td'] = df['t'].diff()
ax[0].scatter(df['t'],df['td'],s=1)
ax[0].title.set_text('kx')

dfi = pd.read_csv(f'imu.{num}.txt', sep='\t', names=['t','ax','ay','az','gx','gy','gz','mx','my','mz','T'], engine='c')
dfi['td'] = dfi['t'].diff()
ax[1].scatter(dfi['t'],dfi['td'],s=1)
ax[1].title.set_text('imu')

dfb = pd.read_csv(f'bmp.{num}.txt', sep='\t', names=['t','T','p','a'], engine='c')
dfb['td'] = dfb['t'].diff()
ax[2].scatter(dfb['t'],dfb['td'],s=1)
ax[2].title.set_text('bmp')

plt.show()
