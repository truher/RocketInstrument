import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

f, ax=plt.subplots(2,1,sharex=True)

df = pd.read_csv('kx.txt', sep=None, names=['t','x','y','z'])
df['td'] = df['t'].diff()
df['g'] = np.sqrt(df['x']*df['x']+df['y']*df['y']+df['z']*df['z'])
ax[0].scatter(df['t'],df['g'],s=1)
ax[0].title.set_text('kx')

dfb = pd.read_csv('bmp.txt', sep=None, names=['t','T','p','a'])
ax[1].scatter(dfb['t'],dfb['a'],s=1)
ax[1].title.set_text('bmp')

plt.show()
