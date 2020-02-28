import React, { Component } from "react";
import axios from 'axios';
import PropTypes from "prop-types";

class Gpio  extends Component {
  
  constructor(props) {
    super(props);
    this.state = {
      config: []
    }
    this.sliderTimeout = null;
    this.handleModeChange = this.handleModeChange.bind(this);
    this.handlePullUpChange = this.handlePullUpChange.bind(this);
    this.handlePullDownChange = this.handlePullDownChange.bind(this);
    this.handleApplyChanges = this.handleApplyChanges.bind(this);
    this.handleStateChange = this.handleStateChange.bind(this);
    this.handleSliderStateChange = this.handleSliderStateChange.bind(this);
  }

  componentDidMount() {
    axios.get('/config.json')
    .then(res => {
      this.setState({
        config: res.data.gpio
      });
      //console.log(res.data);
    })
  }

  handleModeChange(e) {
    let id = e.target.id.replace("sw", "");
    let idx = this.state.config.findIndex(item => item.id == id)
    if (idx > -1 ) {
      let arr = this.state.config;
      (arr[idx])['mode'] = Number(e.target.value);
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
      (arr[idx])['pull_up'] = Number(e.target.checked);
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
      (arr[idx])['pull_down'] = Number(e.target.checked);
      this.setState({
        config: arr
      })
    }
  }

  handleStateChange(e) {
      let id = e.target.id.replace("state", "");
      axios.post('/api/gpio/state', {
        pin: Number(id),
        state: e.target.checked ? 1 : 0
      })
      .then(res => {
        //console.log(res.data);
      })
  }

  handleSliderStateChange(e) {
    let id = e.target.id.replace("sl", "");
    let value = e.target.value;
    this.sliderTimeout && clearTimeout(this.sliderTimeout);    
    this.sliderTimeout = setTimeout(() => {
      axios.post('/api/gpio/level', {
        pin: Number(id),
        level: Number(value)
      })
      .then(res => {
        //console.log(res.data);
      })
    }, 1000);
  }

  handleApplyChanges() {
    axios.post('/api/gpio', {
      config: this.state.config
    })
    .then(res => {
     
    })
  }

  getStateComponent(item) {
    if (item.mode == 2) {
      return (
        <div>
          <input type="checkbox" className="custom-control-input" defaultChecked="false" id={"state" + item.id} onClick={this.handleStateChange} />
          <label className="custom-control-label" htmlFor={"state" + item.id}>state</label>
        </div>
      )
    }
    if (item.mode == 4) {
      return (
        <div className="slidecontainer">
          <input type="range" min="1" max="8191" value={item.value} className="slider" id={"sl" + item.id} onChange={this.handleSliderStateChange}/>
        </div>
      )
    }
    return null;
  }

  render() {
    let { state } = this;
    let rows = state.config.map((item, index) => {
          return (
              <tr key={item.id}>
                <th scope="col">{item.id}</th>
                <th scope="col">
                  <div className="form-group">
                    <select id={"sw" + item.id} value={item.mode} className="form-control" onChange={this.handleModeChange}>
                      <option value="0">Disabled</option>
                      <option value="1">Input</option>
                      <option value="2">Output</option>
                      <option value="4">PWM</option>
                    </select>
                  </div>
                </th>
                <th scope="col">
                  <div className="custom-control custom-switch">
                    {item.mode > 0 ? (
                      <div>
                        <input type="checkbox" className="custom-control-input" defaultChecked={item.pull_up} id={"p_up" + item.id} onClick={this.handlePullUpChange} />
                        <label className="custom-control-label" htmlFor={"p_up" + item.id}>enable</label>
                      </div>
                    ) : (
                      null
                    )
                    }
                  </div>
                </th>
                <th scope="col">
                  <div className="custom-control custom-switch">
                  {item.mode > 0 ? (
                      <div>
                        <input type="checkbox" className="custom-control-input" defaultChecked={item.pull_down} id={"p_dn" + item.id} onClick={this.handlePullDownChange} />
                        <label className="custom-control-label" htmlFor={"p_dn" + item.id}>enable</label>
                      </div>
                  ) : (
                    null
                  )
                  }
                  </div>
                </th>
                <th scope="col">
                  <div className="custom-control custom-switch">
                    {this.getStateComponent(item)}
                  </div>
                </th>
              </tr>
          )
    });

    return(
      <div className="container">
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