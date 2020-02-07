import React, { Component } from "react";
import axios from 'axios';
import PropTypes from "prop-types";

class Mqtt extends Component {

  constructor(props) {
    super(props);
    this.state = {
      config: {
        enable: false,
        broker: "",
        host: ""
      },
      error: null

    }
    this.handleMqttEnable = this.handleMqttEnable.bind(this);
    this.handleBrokerChange = this.handleBrokerChange.bind(this);
    this.handleMqttHostChange = this.handleMqttHostChange.bind(this);
    this.handleApplyChanges = this.handleApplyChanges.bind(this);
  }

  componentDidMount() {
    axios.get('/config.json')
    .then(res => {
      this.setState( {
        config: res.data.mqtt
      });
      console.log(res.data);
    })
  }

  handleApplyChanges() {
    let { config } = this.state;
    axios.post('/api/mqtt', {
      "mqtt" : config
    })
    .then(res => {
      this.setState({
        error: null
      })
      console.log("Updated with res: " + res.data);
    })
    .catch((error) => {
      this.setState({
        error: error.response.status + " " + error.data
      })
    })
  }

  handleMqttEnable(e) {
    let newValue = e.target.checked
    this.setState(prevState => ({
      config: {
        ...prevState.config,
        enable: newValue
      }
    }));
  }

  handleBrokerChange(e) {
    let newValue = e.target.value
    this.setState(prevState => ({
      config: {
        ...prevState.config,
        broker: newValue
      }
    }));
  }

  handleMqttHostChange(e) {
    let newValue = e.target.value
    this.setState(prevState => ({
      config: {
        ...prevState.config,
        host: newValue
      }
    }));
  }

  render() {
    let { config } = this.state;
    let error = null;
    if(this.state.error) {
      error =  (
        <div className="alert alert-danger alert-dismissible fade show">
          <strong>Error!</strong> A problem has been occurred while submitting your data. {this.state.error}
          <button type="button" class="close" data-dismiss="alert">&times;</button>
        </div>
      );
    } 
    return (
          <div className="container">
                <div className="form-group">
                    <div className="form-check">
                        <input className="form-check-input" type="checkbox" id="enableMqtt" checked={config.enable} onChange={this.handleMqttEnable}/>
                        <label className="form-check-label" htmlFor="enableModbus">
                            Enable MQTT
                        </label>
                    </div>
                </div>
                <div className="input-group mb-3">
                    <div className="input-group-prepend">
                        <span className="input-group-text" id="inputGroup-sizing-default">Brocker</span>
                    </div>
                    <input type="text" className="form-control" id="brokerUrl" value={config.broker} onChange={this.handleBrokerChange}/>
               </div>
               <div className="input-group mb-3">
                    <div className="input-group-prepend">
                        <span className="input-group-text" id="inputGroup-sizing-default">Host</span>
                    </div>
                    <input type="text" className="form-control" id="mqttHost" value={config.host} onChange={this.handleMqttHostChange}/>
               </div>
              <button type="button" className="btn btn-danger" onClick={this.handleApplyChanges}>Apply changes</button>
              {error}
          </div>
    );
  }

}

export default Mqtt;