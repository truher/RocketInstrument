import pandas as pd
import matplotlib.pyplot as plt

f, ax=plt.subplots(2,1,sharex=True)

df = pd.read_csv('kx.txt', sep=None, names=['t','x','y','z'])
df['td'] = df['t'].diff()
ax[0].scatter(df['t'],df['td'],s=1)
ax[0].title.set_text('kx')

dfb = pd.read_csv('bmp.txt', sep=None, names=['t','T','p','a'])
dfb['td'] = dfb['t'].diff()
ax[1].scatter(dfb['t'],dfb['td'],s=1)
ax[1].title.set_text('bmp')

plt.show()
