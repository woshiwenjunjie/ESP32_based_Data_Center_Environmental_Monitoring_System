// app.js
App({
  onLaunch() {
    // 展示本地存储能力
    const logs = wx.getStorageSync('logs') || []
    logs.unshift(Date.now())
    wx.setStorageSync('logs', logs)

    // 登录
    wx.login({
      success: res => {
        // 发送 res.code 到后台换取 openId, sessionKey, unionId
      }
    })
  },
  globalData: {
    userInfo: null,
    esp32Url: 'http://192.168.221.108',
    esp32Ip: '192.168.221.108',
    esp32Port: 80,
    aiServiceUrl: 'http://localhost:5000'
  }
})
