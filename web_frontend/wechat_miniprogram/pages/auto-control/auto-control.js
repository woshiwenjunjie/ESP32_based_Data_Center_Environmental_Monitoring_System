// pages/auto-control/auto-control.js
Page({

  /**
   * 页面的初始数据
   */
  data: {
    // 系统状态
    autoControlEnabled: true,
    ruleCount: 0,
    maxRules: 10,
    
    // 规则列表
    rules: [],
    
    // 名称映射
    sensorNames: {
      'temperature': '温度',
      'humidity': '湿度',
      'gas': '气体浓度',
      'smoke': '烟雾',
      'water': '水位'
    },
    conditionNames: {
      '>': '大于',
      '<': '小于',
      '=': '等于',
      'in_range': '在范围内',
      'out_of_range': '超出范围'
    },
    actionNames: {
      'relay_on': '继电器开启',
      'relay_off': '继电器关闭',
      'relay_toggle': '继电器切换',
      'buzzer_on': '蜂鸣器开启',
      'buzzer_off': '蜂鸣器关闭',
      'buzzer_beep': '蜂鸣器短鸣',
      'motor_run': '电机运行',
      'motor_stop': '电机停止',
      'servo_write': '舵机转动',
      'none': '无动作'
    },
    
    // 选项
    sensorOptions: ['温度', '湿度', '气体浓度', '烟雾', '水位'],
    conditionOptions: ['大于', '小于', '等于', '在范围内', '超出范围'],
    actionOptions: ['继电器开启', '继电器关闭', '继电器切换', '蜂鸣器开启', '蜂鸣器关闭', '蜂鸣器短鸣', '电机运行', '电机停止', '舵机转动', '无动作'],
    
    // 传感器类型映射
    sensorTypeMap: ['temperature', 'humidity', 'gas', 'smoke', 'water'],
    conditionMap: ['>', '<', '=', 'in_range', 'out_of_range'],
    actionMap: ['relay_on', 'relay_off', 'relay_toggle', 'buzzer_on', 'buzzer_off', 'buzzer_beep', 'motor_run', 'motor_stop', 'servo_write', 'none'],
    
    // 弹窗状态
    showModal: false,
    editingRuleId: null,
    
    // 表单数据
    form: {
      name: '',
      sensorIndex: 0,
      conditionIndex: 0,
      threshold1: '',
      threshold2: '',
      debounceMs: 1000,
      actionIndex: 0,
      actionParam: '',
      durationMs: 500
    },
    
    // ESP32地址
    esp32Url: ''
  },

  /**
   * 生命周期函数--监听页面加载
   */
  onLoad(options) {
    // 获取ESP32地址
    const app = getApp();
    this.setData({
      esp32Url: app.globalData.esp32Url || 'http://192.168.1.100'
    });
    
    // 先从本地加载缓存的规则
    this.loadRulesFromStorage();
    
    // 然后从ESP32同步最新规则
    this.syncRulesFromDevice();
  },

  /**
   * 生命周期函数--监听页面显示
   */
  onShow() {
    // 刷新规则状态
    this.syncRulesFromDevice();
  },

  /**
   * 从本地存储加载规则
   */
  loadRulesFromStorage() {
    try {
      const rules = wx.getStorageSync('auto_control_rules');
      if (rules && rules.length > 0) {
        this.setData({ rules: rules });
        console.log('[AutoControl] 从本地加载规则:', rules.length);
      }
    } catch (e) {
      console.error('[AutoControl] 本地加载失败:', e);
    }
  },

  /**
   * 保存规则到本地存储
   */
  saveRulesToStorage(rules) {
    try {
      wx.setStorageSync('auto_control_rules', rules);
      console.log('[AutoControl] 规则已保存到本地');
    } catch (e) {
      console.error('[AutoControl] 本地保存失败:', e);
    }
  },

  /**
   * 从设备同步规则
   */
  syncRulesFromDevice() {
    wx.request({
      url: `${this.data.esp32Url}/auto_control`,
      method: 'GET',
      success: (res) => {
        if (res.statusCode === 200 && res.data) {
          const data = typeof res.data === 'string' ? JSON.parse(res.data) : res.data;
          this.setData({
            autoControlEnabled: data.enabled,
            ruleCount: data.rule_count,
            maxRules: data.max_rules,
            rules: data.rules || []
          });
          // 同步到本地存储
          this.saveRulesToStorage(data.rules || []);
        }
      },
      fail: (err) => {
        console.error('[AutoControl] 同步失败:', err);
        // 如果同步失败，使用本地缓存
        this.loadRulesFromStorage();
      }
    });
  },

  /**
   * 加载规则列表（已废弃，使用syncRulesFromDevice）
   */
  loadRules() {
    this.syncRulesFromDevice();
  },

  /**
   * 切换自动控制开关
   */
  onToggleAutoControl(e) {
    const enabled = e.detail.value;
    
    // 防止重复触发
    if (this.data._togglingSystem) {
      return;
    }
    this.setData({ _togglingSystem: true, autoControlEnabled: enabled });
    
    // TODO: 调用API启用/禁用整个自动控制系统
    // 目前仅更新本地状态，后续可添加系统级控制API
    
    wx.showToast({
      title: enabled ? '自动监控已开启' : '自动监控已暂停',
      icon: 'none',
      duration: 1000
    });
    
    // 清除防抖动标记
    setTimeout(() => {
      this.setData({ _togglingSystem: false });
    }, 300);
  },

  /**
   * 切换规则启用状态
   */
  onToggleRule(e) {
    const id = e.currentTarget.dataset.id;
    const enabled = e.detail.value;
    
    // 防止重复触发
    if (this.data._togglingRuleId === id) {
      return;
    }
    this.setData({ _togglingRuleId: id });
    
    // 立即更新本地状态，提供即时反馈
    const rules = this.data.rules.map(rule => {
      if (rule.id === id) {
        return { ...rule, enabled: enabled };
      }
      return rule;
    });
    this.setData({ rules: rules });
    
    const action = enabled ? 'enable' : 'disable';
    
    wx.request({
      url: `${this.data.esp32Url}/auto_control`,
      method: 'POST',
      data: {
        action: action,
        id: id
      },
      success: (res) => {
        if (res.data && res.data.success) {
          wx.showToast({
            title: enabled ? '规则已启用' : '规则已禁用',
            icon: 'success',
            duration: 1000
          });
          // 延迟后同步最新状态
          setTimeout(() => {
            this.syncRulesFromDevice();
          }, 500);
        } else {
          // 失败时恢复原状态
          this.syncRulesFromDevice();
          wx.showToast({
            title: res.data.error || '操作失败',
            icon: 'none'
          });
        }
      },
      fail: () => {
        // 网络失败时恢复原状态
        this.syncRulesFromDevice();
        wx.showToast({
          title: '网络错误',
          icon: 'none'
        });
      },
      complete: () => {
        // 清除防抖动标记
        setTimeout(() => {
          this.setData({ _togglingRuleId: null });
        }, 300);
      }
    });
  },

  /**
   * 显示添加规则弹窗
   */
  showAddRule() {
    if (this.data.ruleCount >= this.data.maxRules) {
      wx.showToast({
        title: '规则数量已达上限',
        icon: 'none'
      });
      return;
    }
    
    this.setData({
      showModal: true,
      editingRuleId: null,
      form: {
        name: '',
        sensorIndex: 0,
        conditionIndex: 0,
        threshold1: '',
        threshold2: '',
        debounceMs: 1000,
        actionIndex: 0,
        actionParam: '',
        durationMs: 500
      }
    });
  },

  /**
   * 编辑规则
   */
  editRule(e) {
    const id = e.currentTarget.dataset.id;
    const rule = this.data.rules.find(r => r.id === id);
    
    if (!rule) return;
    
    // 查找索引
    const sensorIndex = this.data.sensorTypeMap.indexOf(rule.sensor);
    const conditionIndex = this.data.conditionMap.indexOf(rule.condition);
    const actionIndex = this.data.actionMap.indexOf(rule.action);
    
    this.setData({
      showModal: true,
      editingRuleId: id,
      form: {
        name: rule.name,
        sensorIndex: sensorIndex >= 0 ? sensorIndex : 0,
        conditionIndex: conditionIndex >= 0 ? conditionIndex : 0,
        threshold1: String(rule.threshold1),
        threshold2: String(rule.threshold2),
        debounceMs: rule.debounce_ms || 1000,
        actionIndex: actionIndex >= 0 ? actionIndex : 0,
        actionParam: rule.action_param ? String(rule.action_param) : '',
        durationMs: rule.duration_ms || 500
      }
    });
  },

  /**
   * 隐藏弹窗
   */
  hideModal() {
    this.setData({ showModal: false });
  },

  /**
   * 阻止冒泡
   */
  preventHide() {
    // 阻止事件冒泡
  },

  /**
   * 表单输入处理
   */
  onNameInput(e) {
    this.setData({ 'form.name': e.detail.value });
  },

  onSensorChange(e) {
    this.setData({ 'form.sensorIndex': e.detail.value });
  },

  onConditionChange(e) {
    this.setData({ 'form.conditionIndex': e.detail.value });
  },

  onThreshold1Input(e) {
    this.setData({ 'form.threshold1': e.detail.value });
  },

  onThreshold2Input(e) {
    this.setData({ 'form.threshold2': e.detail.value });
  },

  onDebounceChange(e) {
    this.setData({ 'form.debounceMs': e.detail.value });
  },

  onActionChange(e) {
    this.setData({ 'form.actionIndex': e.detail.value });
  },

  onActionParamInput(e) {
    this.setData({ 'form.actionParam': e.detail.value });
  },

  onDurationChange(e) {
    this.setData({ 'form.durationMs': e.detail.value });
  },

  /**
   * 保存规则
   */
  saveRule() {
    const { form, editingRuleId, sensorTypeMap, conditionMap, actionMap } = this.data;
    
    // 验证
    if (!form.name.trim()) {
      wx.showToast({ title: '请输入规则名称', icon: 'none' });
      return;
    }
    
    if (!form.threshold1) {
      wx.showToast({ title: '请输入阈值', icon: 'none' });
      return;
    }
    
    // 构建规则数据
    const ruleData = {
      name: form.name,
      sensor: sensorTypeMap[form.sensorIndex],
      condition: conditionMap[form.conditionIndex],
      threshold1: parseFloat(form.threshold1),
      threshold2: parseFloat(form.threshold2) || 0,
      debounce_ms: form.debounceMs,
      action: actionMap[form.actionIndex],
      action_param: parseInt(form.actionParam) || 0,
      duration_ms: form.durationMs,
      enabled: true
    };
    
    // 范围条件需要阈值2
    if (form.conditionIndex >= 3 && !form.threshold2) {
      wx.showToast({ title: '请输入阈值2', icon: 'none' });
      return;
    }
    
    const requestData = {
      action: editingRuleId ? 'update' : 'add',
      rule: ruleData
    };
    
    if (editingRuleId) {
      requestData.id = editingRuleId;
    }
    
    wx.request({
      url: `${this.data.esp32Url}/auto_control`,
      method: 'POST',
      data: requestData,
      success: (res) => {
        if (res.data && res.data.success) {
          wx.showToast({
            title: editingRuleId ? '规则已更新' : '规则已添加',
            icon: 'success'
          });
          this.hideModal();
          this.loadRules();
        } else {
          wx.showToast({
            title: res.data.error || '保存失败',
            icon: 'none'
          });
        }
      },
      fail: () => {
        wx.showToast({
          title: '网络错误',
          icon: 'none'
        });
      }
    });
  },

  /**
   * 删除规则
   */
  deleteRule(e) {
    const id = e.currentTarget.dataset.id;
    
    wx.showModal({
      title: '确认删除',
      content: '确定要删除这条规则吗？',
      confirmColor: '#ef4444',
      success: (res) => {
        if (res.confirm) {
          wx.request({
            url: `${this.data.esp32Url}/auto_control`,
            method: 'POST',
            data: {
              action: 'delete',
              id: id
            },
            success: (res) => {
              if (res.data && res.data.success) {
                wx.showToast({
                  title: '已删除',
                  icon: 'success'
                });
                this.loadRules();
              } else {
                wx.showToast({
                  title: res.data.error || '删除失败',
                  icon: 'none'
                });
              }
            },
            fail: () => {
              wx.showToast({
                title: '网络错误',
                icon: 'none'
              });
            }
          });
        }
      }
    });
  },

  /**
   * 测试规则
   */
  testRule(e) {
    const id = e.currentTarget.dataset.id;
    
    wx.request({
      url: `${this.data.esp32Url}/auto_control`,
      method: 'POST',
      data: {
        action: 'trigger',
        id: id
      },
      success: (res) => {
        if (res.data && res.data.success) {
          wx.showToast({
            title: '已触发',
            icon: 'success'
          });
        } else {
          wx.showToast({
            title: res.data.error || '触发失败',
            icon: 'none'
          });
        }
      },
      fail: () => {
        wx.showToast({
          title: '网络错误',
          icon: 'none'
        });
      }
    });
  },

  /**
   * 加载默认规则
   */
  loadDefaultRules() {
    wx.showModal({
      title: '加载默认规则',
      content: '这将清除现有规则并加载5条默认规则，确定继续吗？',
      success: (res) => {
        if (res.confirm) {
          wx.request({
            url: `${this.data.esp32Url}/auto_control`,
            method: 'POST',
            data: {
              action: 'load_defaults'
            },
            success: (res) => {
              if (res.data && res.data.success) {
                wx.showToast({
                  title: '默认规则已加载',
                  icon: 'success'
                });
                this.loadRules();
              } else {
                wx.showToast({
                  title: res.data.error || '加载失败',
                  icon: 'none'
                });
              }
            },
            fail: () => {
              wx.showToast({
                title: '网络错误',
                icon: 'none'
              });
            }
          });
        }
      }
    });
  },

  /**
   * 下拉刷新
   */
  onPullDownRefresh() {
    this.syncRulesFromDevice();
    wx.stopPullDownRefresh();
  },

  /**
   * 保存配置到ESP32
   */
  saveConfigToDevice() {
    wx.showLoading({ title: '保存中...' });
    
    wx.request({
      url: `${this.data.esp32Url}/auto_control`,
      method: 'POST',
      data: {
        action: 'save'
      },
      success: (res) => {
        wx.hideLoading();
        if (res.data && res.data.success) {
          wx.showToast({
            title: '配置已保存',
            icon: 'success'
          });
          // 同时保存到本地
          this.saveRulesToStorage(this.data.rules);
        } else {
          wx.showToast({
            title: res.data.error || '保存失败',
            icon: 'none'
          });
        }
      },
      fail: () => {
        wx.hideLoading();
        wx.showToast({
          title: '网络错误',
          icon: 'none'
        });
      }
    });
  },

  /**
   * 从ESP32加载配置
   */
  loadConfigFromDevice() {
    wx.showModal({
      title: '加载配置',
      content: '从设备加载保存的规则配置？',
      success: (res) => {
        if (res.confirm) {
          wx.showLoading({ title: '加载中...' });
          
          wx.request({
            url: `${this.data.esp32Url}/auto_control`,
            method: 'POST',
            data: {
              action: 'load'
            },
            success: (res) => {
              wx.hideLoading();
              if (res.data && res.data.success) {
                wx.showToast({
                  title: `已加载${res.data.count}条规则`,
                  icon: 'success'
                });
                this.syncRulesFromDevice();
              } else {
                wx.showToast({
                  title: res.data.error || '无保存的配置',
                  icon: 'none'
                });
              }
            },
            fail: () => {
              wx.hideLoading();
              wx.showToast({
                title: '网络错误',
                icon: 'none'
              });
            }
          });
        }
      }
    });
  },

  /**
   * 手动同步配置
   */
  manualSync() {
    wx.showLoading({ title: '同步中...' });
    
    // 先保存到设备
    wx.request({
      url: `${this.data.esp32Url}/auto_control`,
      method: 'POST',
      data: {
        action: 'save'
      },
      success: (res) => {
        if (res.data && res.data.success) {
          // 保存本地
          this.saveRulesToStorage(this.data.rules);
          wx.hideLoading();
          wx.showToast({
            title: '同步成功',
            icon: 'success'
          });
        } else {
          wx.hideLoading();
          wx.showToast({
            title: '同步失败',
            icon: 'none'
          });
        }
      },
      fail: () => {
        wx.hideLoading();
        wx.showToast({
          title: '网络错误',
          icon: 'none'
        });
      }
    });
  }
});
