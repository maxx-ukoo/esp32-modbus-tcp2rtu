import React, { Component } from "react";
import axios from 'axios';
import PropTypes from "prop-types";

class Gpio  extends Component {
  
  constructor(props) {
    super(props);
    this.state = {
      enable: false,
      config: []
    }
    this.handleModeChange = this.handleModeChange.bind(this);
    this.handlePullUpChange = this.handlePullUpChange.bind(this);
    this.handlePullDownChange = this.handlePullDownChange.bind(this);
    this.handleApplyChanges = this.handleApplyChanges.bind(this);
    this.handleGpioEnable = this.handleGpioEnable.bind(this);
  }

  componentDidMount() {
    axios.get('/config.json')
    .then(res => {
      this.setState({
        enable: res.data.gpio.enable,
        config: res.data.gpio.config
      });
      console.log(res.data);
    })
  }

  handleGpioEnable(e) {
    this.setState({enable: e.target.checked});
  }

  handleModeChange(e) {
    let id = e.target.id.replace("sw", "");
    let idx = this.state.config.findIndex(item => item.id == id)
    if (idx > -1 ) {
      let arr = this.state.config;
      (arr[idx])['mode'] = e.target.checked;
      this.setState({
        config: arr
      })
    }
  }

  handlePullUpChange(e) {
    let id = e.target.id.replace("p_up", "");
    let idx = this.state.config.findIndex(item => item.id == id)
    if (idx > -1 ) {
      let arr = this.state.config;
      (arr[idx])['pull_up'] = e.target.checked;
      this.setState({
        config: arr
      })
    }
  }

  handlePullDownChange(e) {
    let id = e.target.id.replace("p_dn", "");
    let idx = this.state.config.findIndex(item => item.id == id)
    if (idx > -1 ) {
      let arr = this.state.config;
      (arr[idx])['pull_down'] = e.target.checked;
      this.setState({
        config: arr
      })
    }
  }

  handleApplyChanges() {
    axios.post('/api/gpio', {
      enable: this.state.enable,
      config: this.state.config
    })
    .then(res => {
      console.log("rebooting");
      axios.post('/api/v1/system/reboot')
      .then(res => {
          
      })
    })
  }

  render() {
    let { state } = this;

    let rows = state.config.map((item, index) => {
          return (
              <tr key={item.id}>
                <th scope="col">{item.id}</th>
                <th scope="col">
                  <div className="custom-control custom-switch">
                      <input type="checkbox" className="custom-control-input" defaultChecked={item.mode} id={"sw" + item.id} onClick={this.handleModeChange} />
                      <label className="custom-control-label" htmlFor={"sw" + item.id}>Output</label>
                  </div>
                </th>
                <th scope="col">
                  <div className="custom-control custom-switch">
                      <input type="checkbox" className="custom-control-input" defaultChecked={item.pull_up} id={"p_up" + item.id} onClick={this.handlePullUpChange} />
                      <label className="custom-control-label" htmlFor={"p_up" + item.id}>enable</label>
                  </div>
                </th>
                <th scope="col">
                  <div className="custom-control custom-switch">
                      <input type="checkbox" className="custom-control-input" defaultChecked={item.pull_down} id={"p_dn" + item.id} onClick={this.handlePullDownChange} />
                      <label className="custom-control-label" htmlFor={"p_dn" + item.id}>enable</label>
                  </div>
                </th>
          <th scope="col">{item.state}</th>
              </tr>
          )
    });

    return(
      <div className="container">
        <div className="form-group">
              <div className="form-check">
                <input className="form-check-input" type="checkbox" id="enableGpio" defaultChecked={state.enable} onClick={this.handleGpioEnable}/>
                <label className="form-check-label" htmlFor="enableGpio">
                  Enable GPIO
                </label>
              </div>
            </div>
        <table className="table">
            <thead className="thead-dark">
              <tr>
                <th scope="col">#</th>
                <th scope="col">Mode</th>
                <th scope="col">Pull Up</th>
                <th scope="col">Pull Down</th>
                <th scope="col">State</th>
              </tr>
            </thead>
            <tbody>
              {rows}
            </tbody>
          </table>
          <button type="button" className="btn btn-danger" onClick={this.handleApplyChanges}>Apply changes</button>
      </div>
    )
  }
}
   

export default Gpio;