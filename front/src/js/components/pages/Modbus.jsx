import React, { Component } from "react";
import axios from 'axios';
import PropTypes from "prop-types";

class Modbus extends Component {

  constructor(props) {
    super(props);
    this.state = {
      enable: false,
      speed: 9600
    }
    this.handleModBusEnable = this.handleModBusEnable.bind(this);
    this.handleModBusSpeedChange = this.handleModBusSpeedChange.bind(this);
    this.updateConfig = this.updateConfig.bind(this);
  }

  componentDidMount() {
    axios.get('/config.json')
    .then(res => {
      this.setState({
        enable: res.data.modbus.enable,
        speed: res.data.modbus.speed
      });
      console.log(res.data);
    })
  }

  updateConfig() {
    axios.post('/api/modbus', {
      enable: this.state.enable,
      speed: this.state.speed
    })
    .then(res => {
        console.log(res.data);
    })
  }

  handleModBusEnable(e) {
    this.setState({enable: e.target.checked}, () => this.updateConfig());
  }

  handleModBusSpeedChange(e) {
    this.setState({speed: Number(e.target.value)}, () => this.updateConfig());
  }

  handleApplyChanges() {
    console.log("rebooting");
    axios.post('/api/v1/system/reboot')
    .then(res => {
      console.log("rebooting");
      axios.post('/api/v1/system/reboot')
      .then(res => {
      })
    })
  }

  render() {
    let { state } = this;
    return (
          <div className="container">
            <div className="form-group">
              <label htmlFor="modbusSpeed">Port speed</label>
              <select id="modbusSpeed" className="form-control" value={state.speed} onChange={this.handleModBusSpeedChange}>
                <option>9600</option>
                <option>115200</option>
              </select>
            </div>
            <div className="form-group">
              <div className="form-check">
                <input className="form-check-input" type="checkbox" id="enableModbus" checked={state.enable} onClick={this.handleModBusEnable}/>
                <label className="form-check-label" htmlFor="enableModbus">
                  Enable ModBus
                </label>
              </div>
            </div>
            <button type="button" className="btn btn-danger" onClick={this.handleApplyChanges}>Apply changes</button>
          </div>
    );
  }

}

export default Modbus;