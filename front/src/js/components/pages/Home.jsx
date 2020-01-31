import React, { Component } from "react";
import axios from 'axios';
import PropTypes from "prop-types";

class Home extends Component {

    constructor(props) {
        super(props);
        this.state = {
          model: "",
          version: "",
          cores: "",
          date: "",
          time: "",
          idf_ver: "",
          uptime: "",
          memory: 0
        }
    }

    componentDidMount() {
      axios.get('/api/v1/system/info')
      .then(res => {
          this.setState({
            model: res.data.model,
            version: res.data.version,
            cores: res.data.cores,
            date: res.data.date,
            time: res.data.time,
            idf_ver: res.data.idf_ver,
            uptime: res.data.uptime,
            memory: res.data.memory
          });
      })
    }

    render() {
      let { state } = this;
      return (
          <div className="jumbotron">
            <h1>IOT Module</h1> 
              <p><strong>ESP Model:</strong> {state.model}, <strong>Cores:</strong> {state.cores}</p>
              <p><strong>Version:</strong> {state.version}</p>
              <p><strong>Build time:</strong> {state.date}-{state.time}</p>
              <p><strong>Idf version:</strong> {state.idf_ver}</p>
              <p><strong>Uptime:</strong> {state.uptime}</p>
              <p><strong>Memory available:</strong> {state.memory}</p>
            </div>
        );
    }
  }

export default Home;
